#!/usr/bin/perl

use strict;
use warnings;

use Date::Parse;
use Errno qw(ESRCH);
use File::Path qw(remove_tree);
use File::stat;

$, = " ";
$\ = "\n";

my $usage = "usage: $0 <directory> <delete ts min> [<delete ts max>]\n";

# Always one root at a time, local or archive, never both: gc on one must
# never depend on the other being present, mounted, or ever having existed,
# and ordinarily only local needs to run at all, since archive content is
# not expected to change once written.
my $directory = $ARGV[0];
die($usage) unless $directory;
$directory =~ s|/+$||;

my $delete_ts_min = $ARGV[1] || "2021-01-01";
die($usage) unless $delete_ts_min;
$delete_ts_min = str2time($delete_ts_min);

my $delete_ts_max = $ARGV[2];
if($delete_ts_max)
{
    $delete_ts_max = str2time($delete_ts_max);
}
elsif(-d "$directory/move_nodes")
{
    opendir(MOVE_NODES_DIR, "$directory/move_nodes") || die("error opening $directory/move_nodes : $!\n");
    for my $save_link (readdir(MOVE_NODES_DIR))
    {
        my $save_link_full = "$directory/move_nodes/" . $save_link;
        next unless -l $save_link_full;

        my $mtime = lstat($save_link_full)->mtime;

        $delete_ts_max = $mtime unless $delete_ts_max;
        $delete_ts_max = $mtime if $mtime > $delete_ts_max;
    }
    closedir(MOVE_NODES_DIR);
}
die("no <delete ts max> given, and $directory has no move_nodes to default one from\n" . $usage) unless $delete_ts_max;

# A saved DFA is one file, <directory>/dfas_by_hash/<sha256>.dfa, and every
# other name for it is a symbolic link to that file. Both halves of this
# script key on the bare hash, so they cannot drift apart the way they did
# when the file naming changed: the scan below reads names, the loop after it
# reads link targets, and both go through here.
sub dfa_file_hash
{
  my ($file_name) = @_;

  return ($file_name =~ /^([0-9a-f]{64})\.dfa$/) ? $1 : undef;
}

# True only once a pid is confirmed gone (ESRCH). Any other outcome -- still
# running, or kill(0) refused for some other reason -- means leave it alone:
# deleting a live build's own staging directory out from under it is worse
# than leaving a dead one around a little longer.
sub pid_is_gone
{
  my ($pid) = @_;
  return 0 if kill(0, $pid);
  return $! == ESRCH;
}

# Keep set: every hash any non-cache, non-staging directory under $directory
# still names. On archive this is the per-game result directories; on local
# it is move_nodes (the op-caches and staging directories below are handled
# on their own, without reference counting).
my %keep_hashes;

opendir(ROOT_DIR, $directory) || die("error opening $directory : $!\n");
for my $save_dir (readdir(ROOT_DIR))
{
  next unless $save_dir =~ /^[a-z]/;
  next if $save_dir =~ /_cache$/;
  next if $save_dir eq "dfas_by_hash";
  next if $save_dir eq "build";
  next if $save_dir eq "binarydfa";
  next if $save_dir eq "sizes";

  my $save_full = "$directory/" . $save_dir;
  next unless -d $save_full;

  print("scanning", $save_dir);
  opendir(SAVE_DIR, $save_full) || die("error opening $save_full : $!\n");
  for my $save_link (readdir(SAVE_DIR))
  {
    my $save_link_full = $save_full . "/" . $save_link;
    next unless -l $save_link_full;

    my $link_target = readlink($save_link_full);
    defined($link_target) || die("error reading link $save_link_full : $!\n");

    my $target_name = $link_target;
    $target_name =~ s|.*/||;

    # A name this does not recognize must stop the run rather than be skipped.
    # Skipping drops a hash from the keep set, and a hash missing from the keep
    # set is a live DFA deleted below without a word about it.
    my $dfa_hash = dfa_file_hash($target_name);
    defined($dfa_hash) ||
      die("$save_link_full points at \"$link_target\", which is not a DFA file\n");

    $keep_hashes{$dfa_hash} = 1;
  }
  closedir(SAVE_DIR);
}
closedir(ROOT_DIR);
print(scalar(keys(%keep_hashes)), "DFAs to keep");

# dfas_by_hash sweep -- only if this root actually has one. archive always
# will; local will not until it grows its own read-through mirror of
# archive's content.
if(-d "$directory/dfas_by_hash")
{
  # An empty keep set means every DFA in the window below is about to be
  # deleted. That is what a run from the wrong directory looks like, and it
  # is far more likely than a root that genuinely names nothing, so stop and
  # say so.
  if(!%keep_hashes)
  {
    die("no DFA is named by any directory under $directory, so everything would be deleted; refusing\n");
  }

  my $total_count = 0;
  my $unsaved_count = 0;

  my @delete_hashes;
  opendir(DFAS_BY_HASH_DIR, "$directory/dfas_by_hash") || die("error opening dfas_by_hash : $!\n");
  for my $dfa_file (readdir(DFAS_BY_HASH_DIR))
  {
    # Anything else here is not a saved DFA: "." and "..", and the
    # .tmp-<pid>-<n>.dfa files save_by_hash writes before it knows the digest.
    my $dfa_hash = dfa_file_hash($dfa_file);
    next unless defined($dfa_hash);
    ++$total_count;

    next if exists($keep_hashes{$dfa_hash});
    ++$unsaved_count;

    my $dfa_file_full = "$directory/dfas_by_hash/" . $dfa_file;

    my $stat = stat($dfa_file_full);
    $stat || die("error reading $dfa_file_full : $!\n");

    my $mtime = $stat->mtime;
    next unless $delete_ts_min <= $mtime and $mtime <= $delete_ts_max;

    push(@delete_hashes, $dfa_hash);
  }
  closedir(DFAS_BY_HASH_DIR);

  print($total_count, "DFAs found");
  print($unsaved_count, "DFAs not saved");
  print(scalar(@delete_hashes), "DFAs to delete");

  for my $dfa_hash (@delete_hashes)
  {
    my $dfa_file_full = "$directory/dfas_by_hash/" . $dfa_hash . ".dfa";
    unlink($dfa_file_full) || die("error deleting $dfa_file_full : $!\n");
  }
}

# Stale construction staging: build/<pid>-<n> always, binarydfa/<pid>-<n>
# once binarydfa is pid-scoped. Swept by pid liveness, not age or reference
# counting -- a legitimately long-running build must never lose its own
# working directory out from under it, which an age cutoff cannot promise
# but a liveness check can.
for my $staging_dir ("build", "binarydfa")
{
  my $staging_full = "$directory/$staging_dir";
  next unless -d $staging_full;

  opendir(STAGING_DIR, $staging_full) || die("error opening $staging_full : $!\n");
  for my $entry (readdir(STAGING_DIR))
  {
    next unless $entry =~ /^(\d+)-\d+$/;
    next unless pid_is_gone($1);

    my $entry_full = "$staging_full/$entry";
    print("removing stale", $entry_full);
    remove_tree($entry_full, { safe => 1 });
  }
  closedir(STAGING_DIR);
}

# Disposable caches: change_cache/, difference_cache/, intersection_cache/,
# inverse_cache/, union_cache/, sizes/. Pure memoization -- nothing durable
# depends on any entry surviving, whether or not its dfas_by_hash target
# still exists -- so age is enough on its own, no reference counting.
opendir(ROOT_DIR, $directory) || die("error opening $directory : $!\n");
for my $cache_dir (readdir(ROOT_DIR))
{
  next unless ($cache_dir =~ /_cache$/) || ($cache_dir eq "sizes");

  my $cache_full = "$directory/$cache_dir";
  next unless -d $cache_full;

  opendir(CACHE_DIR, $cache_full) || die("error opening $cache_full : $!\n");
  for my $entry (readdir(CACHE_DIR))
  {
    next if $entry =~ /^\.\.?$/;

    my $entry_full = "$cache_full/$entry";
    my $stat = lstat($entry_full);
    next unless $stat;

    my $mtime = $stat->mtime;
    next unless $delete_ts_min <= $mtime and $mtime <= $delete_ts_max;

    unlink($entry_full) || die("error deleting $entry_full : $!\n");
  }
  closedir(CACHE_DIR);
}
closedir(ROOT_DIR);
