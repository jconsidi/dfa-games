#!/usr/bin/perl -n

use strict;

our(%counts);

s/layer=[0-9]+ //;
next unless /(.*) took (\d+)s/;

$counts{$1} = 0 unless $counts{$1};
$counts{$1} += int($2);

END
{
    foreach my $key (keys(%counts))
    {
	print($counts{$key}, "\t", $key, "\n");
    }
}
