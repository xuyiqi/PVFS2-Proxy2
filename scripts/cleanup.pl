#!/usr/bin/perl

for (my $i =1; $i<=9; $i++)
{
	$result = "ssh c1n$i killall -w proxy2 IOR mpstat iostat sar vmstat top";
	print "$result\n";
	$r = `$result`;
	print "$r\n";
}

for (my $i =1; $i<=8; $i++)
{
	$result = "ssh c2n$i killall -w proxy2 IOR mpstat iostat sar vmstat top";
	print "$result\n";
	$r = `$result`;
	print "$r\n";
}
