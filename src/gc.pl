#!/usr/bin/perl

use strict;
use warnings;

use Date::Parse;
use File::Path;
use File::stat;

$, = " ";
$\ = "\n";

my $usage = "usage: $0 <delete ts min> <delete ts max>\n";

my $delete_ts_min = $ARGV[0];
die($usage) unless $delete_ts_min;
$delete_ts_min = str2time($delete_ts_min);

my $delete_ts_max = $ARGV[1];
die($usage) unless $delete_ts_max;
$delete_ts_max = str2time($delete_ts_max);

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
  opendir(SAVE_DIR, $save_full) || dir("error opening $save_full : $!\n");
  for my $save_link (readdir(SAVE_DIR))
  {
    my $save_link_full = $save_full . "/" . $save_link;
    next unless -l $save_link_full;

    my $dfa_hash = readlink($save_link_full);
    $dfa_hash =~ s|.*/||;
    $keep_hashes{$dfa_hash} = 1;
  }
}
print(scalar(%keep_hashes), "DFAs to keep");

my $total_count = 0;
my $unsaved_count = 0;

my @delete_hashes;
opendir(DFAS_BY_HASH_DIR, "scratch/dfas_by_hash") || die("error opening dfas_by_hash : $!\n");
for my $dfa_hash (readdir(DFAS_BY_HASH_DIR))
{
  next unless $dfa_hash =~ /^[0-9a-f]{64}$/;
  ++$total_count;

  next if exists($keep_hashes{$dfa_hash});
  ++$unsaved_count;

  my $dfa_hash_full = "scratch/dfas_by_hash/" . $dfa_hash;

  my $mtime = stat($dfa_hash_full)->mtime;
  next unless $delete_ts_min <= $mtime and $mtime <= $delete_ts_max;

  push(@delete_hashes, $dfa_hash);
}

print($total_count, "DFAs found");
print($unsaved_count, "DFAs not saved");
print(scalar(@delete_hashes), "DFAs to delete");

for my $dfa_hash (@delete_hashes)
{
  my $dfa_hash_full = "scratch/dfas_by_hash/" . $dfa_hash;
  rmtree($dfa_hash_full);
}
