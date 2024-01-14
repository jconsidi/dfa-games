#!/usr/bin/perl

use strict;
use warnings;

my $usage = "usage: $0 <GAME> <FORWARD PLY MAX> <BACKWARD PLY MAX>\n";
die($usage) unless scalar(@ARGV) == 3;

my($game, $forward_ply_max, $backward_ply_max) = @ARGV;
my $max_examples = 10000;

die($usage) unless int($forward_ply_max) >= 0;
die($usage) unless int($backward_ply_max) >= 0;

sub run {
    system(@_) && die("$_[0] failed : $!\n");
}

run("./validate_terminal", $game, $max_examples);
run("./build_forward", $game, $forward_ply_max);
run("./validate_forward", $game, $forward_ply_max, $max_examples);
run("./build_backward", $game, $backward_ply_max);
run("./validate_backward", $game, $backward_ply_max, $max_examples);
run("./build_forward_backward", $game, $forward_ply_max, $backward_ply_max);
run("./validate_forward_backward", $game, $forward_ply_max, $backward_ply_max, $max_examples);
run("./stats_forward_backward", $game, $forward_ply_max, $backward_ply_max);
