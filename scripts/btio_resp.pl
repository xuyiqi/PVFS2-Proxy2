#!/usr/bin/perl
$appfile=$ARGV[0];
#print "$appfile\n";
open (CHECKBOOK, $appfile);

print "btio log file is $appfile\n";
$line=0;

$record=<CHECKBOOK>;
open (WEIGHTS,">$appfile.resp");
$zero=0;
while ($record=<CHECKBOOK>)
{
	$line++;
	@recordvalues = split (' ', $record);
	#print "got @recordvalues\n";
	$starttime=$recordvalues[0];
	$endtime=$recordvalues[1];
	
	$resptime=($endtime-$starttime)*1000;
	#print "$resptime = ($endtime - $starttime ) *1000 \n";
	if ($resptime==0 && $zero==1)
	{
		last;
	}
	elsif ($resptime==0)
	{
		$zero++;
		next;
	}
	print WEIGHTS "$resptime,$recordvalues[2],$recordvalues[3],$recordvalues[4]\n";	
}
close(CHECKBOOK);
close(WEIGHTS);
