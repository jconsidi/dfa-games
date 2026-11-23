#!/usr/bin/perl

use strict;
use warnings;

my $usage = "usage: $0 <GAME> <FORWARD PLY MAX> <BACKWARD PLY MAX>\n";
die($usage) unless scalar(@ARGV) == 3;

my($game, $forward_ply_max, $backward_ply_max) = @ARGV;

die($usage) unless int($forward_ply_max) >= 0;
die($usage) unless int($backward_ply_max) >= 0;

sub run {
    system(@_) && die("$_[0] failed : $!\n");
}

run("./build_forward", $game, $forward_ply_max);
run("./verify_forward_exact", $game, $forward_ply_max);
run("./build_backward", $game, $backward_ply_max);
run("./verify_backward", $game, $backward_ply_max);
run("./build_forward_backward", $game, $forward_ply_max, $backward_ply_max);
run("./verify_forward_backward", $game, $forward_ply_max, $backward_ply_max);
run("./stats_forward_backward", $game, $forward_ply_max, $backward_ply_max);
