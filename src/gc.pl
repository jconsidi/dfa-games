#!/usr/bin/perl

use strict;
use warnings;

use Date::Parse;
use File::stat;

$, = " ";
$\ = "\n";

my $usage = "usage: $0 <delete ts min> <delete ts max>\n";

my $delete_ts_min = $ARGV[0] || "2021-01-01";
die($usage) unless $delete_ts_min;
$delete_ts_min = str2time($delete_ts_min);

my $delete_ts_max = $ARGV[1];
if($delete_ts_max)
{
    $delete_ts_max = str2time($delete_ts_max);
}
else
{
    opendir(MOVE_NODES_DIR, "scratch/move_nodes") || die("error opening scratch/move_nodes : $!\n");
    for my $save_link (readdir(MOVE_NODES_DIR))
    {
        my $save_link_full = "scratch/move_nodes/" . $save_link;
        next unless -l $save_link_full;

        my $mtime = lstat($save_link_full)->mtime;

        $delete_ts_max = $mtime unless $delete_ts_max;
        $delete_ts_max = $mtime if $mtime > $delete_ts_max;
    }
}
die($usage) unless $delete_ts_max;

# A saved DFA is one file, scratch/dfas_by_hash/<sha256>.dfa, and every other
# name for it is a symbolic link to that file. Both halves of this script key
# on the bare hash, so they cannot drift apart the way they did when the file
# naming changed: the scan below reads names, the loop after it reads link
# targets, and both go through here.
sub dfa_file_hash
{
  my ($file_name) = @_;

  return ($file_name =~ /^([0-9a-f]{64})\.dfa$/) ? $1 : undef;
}

my %keep_hashes;

opendir(SCRATCH_DIR, "scratch") || die("error opening scratch : $!\n");
for my $save_dir (readdir(SCRATCH_DIR))
{
  next unless $save_dir =~ /^[a-z]/;
  next if $save_dir =~ /_cache$/;
  next if $save_dir eq "dfas_by_hash";

  my $save_full = "scratch/" . $save_dir;
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
}
print(scalar(keys(%keep_hashes)), "DFAs to keep");

# An empty keep set means every DFA in the window below is about to be deleted.
# That is what a run from the wrong directory looks like, and it is far more
# likely than a scratch that genuinely names nothing, so stop and say so.
if(!%keep_hashes)
{
  die("no DFA is named by any directory under scratch, so everything would be deleted; refusing\n");
}

my $total_count = 0;
my $unsaved_count = 0;

my @delete_hashes;
opendir(DFAS_BY_HASH_DIR, "scratch/dfas_by_hash") || die("error opening dfas_by_hash : $!\n");
for my $dfa_file (readdir(DFAS_BY_HASH_DIR))
{
  # Anything else here is not a saved DFA: "." and "..", and the
  # .tmp-<pid>-<n>.dfa files save_by_hash writes before it knows the digest.
  my $dfa_hash = dfa_file_hash($dfa_file);
  next unless defined($dfa_hash);
  ++$total_count;

  next if exists($keep_hashes{$dfa_hash});
  ++$unsaved_count;

  my $dfa_file_full = "scratch/dfas_by_hash/" . $dfa_file;

  my $stat = stat($dfa_file_full);
  $stat || die("error reading $dfa_file_full : $!\n");

  my $mtime = $stat->mtime;
  next unless $delete_ts_min <= $mtime and $mtime <= $delete_ts_max;

  push(@delete_hashes, $dfa_hash);
}

print($total_count, "DFAs found");
print($unsaved_count, "DFAs not saved");
print(scalar(@delete_hashes), "DFAs to delete");

for my $dfa_hash (@delete_hashes)
{
  my $dfa_file_full = "scratch/dfas_by_hash/" . $dfa_hash . ".dfa";
  unlink($dfa_file_full) || die("error deleting $dfa_file_full : $!\n");
}
