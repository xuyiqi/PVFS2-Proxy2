#!/usr/bin/perl
######### Parsing Arguments ###########

$numArgs = $#ARGV + 1;
@paranames = ("Ring","Ranks");
if ($numArgs < 2)
{
	print "error arguments: @paranames\n";
	exit -1;
}

print "$numArgs command-line arguments:\n";

print "############### Outside Parameters #############\n";

foreach $argnum (0 .. $#ARGV) {
   print "# $paranames[$argnum]:\t$ARGV[$argnum]\n";
}

print "############### End Parameters #############\n";

#$dummy = <STDIN>;

print "Starting Merge Log Program.....\n";


$Ring=shift;
$Rank=shift;

print "App $Ring Processes $Rank\n";


$r="rm log.$Ring";
print "$r\n";
$result=`$r`;
print "$result\n";

for ($i=0; $i<$Rank; $i++)
{
	$r="cat log_file_rank_".$Ring."_".$i." | sed '\$d' >> log.$Ring";
	print "$r\n";
	$result=`$r`;
	print "$result\n";
}
