#!/usr/bin/perl

#this script assumes that the file is read in using "time resp" format,
#where the columns are first sorted by time, then by resp
######### Parsing Arguments ###########

$numArgs = $#ARGV + 1;
@paranames = ("Filename","Interval","Math");
if ($numArgs < 3)
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

print "Starting Issue Rate Program.....\n";


$Filename=shift;
$Interval=shift;
$Math=shift;
if ($Math ne 'count' && $Math ne 'sum' && $Math ne 'average' && $Math ne 'percentile')
{
	print "$Math math is not supported: must be count, sum, average, or percentile\n";
	exit 0;
}

if ($Math eq 'percentile')
{
	$Percentile=shift;
}

print "$Filename $Interval seconds, $Math, Percentile $Percentile\n";


open (TRACE, "$Filename");

if ($Math eq "percentile")
{
	open (HISTO, ">$Filename.trace.$Math.$Percentile");
}
else
{
	open (HISTO, ">$Filename.trace.$Math");
}

$line=0;
$first_second=0;
#$record = <TRACE>;
$interval_value=0;
$intervals=0;
$last_time=0;
$count=0;
while ($record = <TRACE>) {
	$line++;
	@line_values= split(' ',$record);
	if ($line==1)
	{
		@line_values= split(' ',$record);
		$first_second = $line_values[0];	
		print "first second is $first_second\n";
	}
	$second= $line_values[0]-$first_second;
	$count++;
	$tp = $line_values[1];
	#print "second $second, tp $tp\n";
	if ($second-$last_time>=$Interval)
	{
		
		
		$tmp=$Interval*$intervals;
		
		if ($Math eq "sum"){
			print HISTO "$tmp\t$interval_value\n";
		}
		if ($Math eq "count"){
			print HISTO "$tmp\t$count\n";
		}
		if ($Math eq "average"){
			$avg = $interval_value/$count;
			print HISTO "$tmp\t$avg\n";
		}
		if ($Math eq "percentile"){
			$per_value=$Percentile*$interval_value;
			#print "total is $interval_value, $Percentile percentile is $per_value, $#last_list items\n";
			$p_sum=0;
			for ($p=0;$p<=$#last_list; $p++)
			{
				$p_sum=$p_sum+$last_list[$p];
				#print "sum is now $p_sum\n";
				if ($p_sum>=$per_value || $#last_list==0)
				{
					#print "sum is now $p_sum, last value found is $last_list[$p]\n";
					print HISTO "$tmp\t$last_list[$p]\n";
					@last_list=();	
					last;
				}

			}
		}
		$intervals++;
		$count=0;
		#figure how many MORE intervals we may have skipped
		$more_intervals = ($second)/$Interval;
		#print "skipping from $intervals to $more_intervals\n";		
		for ($i=0;$i<$more_intervals-$intervals;$i++)
		{
			
			$tmp=$Interval*($intervals+$i);
			print HISTO "$tmp\t0\n";
			
		
		}
		push (@last_list, $tp);		
		$intervals = $more_intervals;
		$last_time=($more_intervals) * $Interval;#nearest second including those skipped
		$interval_value=$tp;
	}
	else
	{
		push (@last_list, $tp);
		$interval_value=$interval_value+$tp;
		#print "accumulated to $interval_value\n";
	}
}
#$intervals++;
$tmp=$Interval*$intervals;

if ($Math eq "sum"){
	print HISTO "$tmp\t$interval_value\n";
}
if ($Math eq "count"){
	print HISTO "$tmp\t$count\n";
}
if ($Math eq "average"){
	$avg = $interval_value/$count;
	print HISTO "$tmp\t$avg\n";
}
if ($Math eq "percentile"){
        $per_value=$Percentile*$interval_value;
        print "total is $interval_value, $Percentile percentile is $per_value, $#last_list items\n";
	$p_sum=0;	
        for ($p=0;$p<=$#last_list; $p++)
        {
 		$p_sum=$p_sum+$last_list[$p];
		#print "sum is now $p_sum\n";
		if ($p_sum>=$per_value || $#last_list==0)
                {
                    #print "last value found is $last_list[$p]\n";
                    print HISTO "$tmp\t$last_list[$p]\n";
                    @last_list=();
                    last;
                }
        }
}


close (TRACE);
close (HISTO);

