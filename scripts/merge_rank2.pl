#!/usr/bin/perl


######### Parsing Arguments ###########

$numArgs = $#ARGV + 1;
@paranames = ("Rings","head","tail","Ring1","Ring2","Ring3","Ring4","Ring5","Ring6","Ring7","Ring8","Ring9");
if ($numArgs < 4)
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

print "Starting Merge Program.....\n";

$log_file_prefix="log_file_rank_";

$Rings=shift;
$head=shift;
$tail=shift;
print "!!!!!!!!!!!!!!!! $Rings rings !!!!!!!!!!!!!!!\n";
$max_start=0;
$min_end=100000000000000;
for ($it=1; $it<=$Rings; $it++)
{
	print "ring $it\n";
	$ranks=$ARGV[$it-1];
	print "ranks $ranks\n";
	for ($rank=1; $rank<=$ranks; $rank++)
	{
		print "rank $rank\n";
                $ra = $rank-1;
                $file= "$log_file_prefix"."$it"."_"."$ra";
		print "opening log $file\n";
		open (CHECKBOOK, "$file");
                $line=0;
		$record = <CHECKBOOK>;
                while ($record = <CHECKBOOK>) {
			$line++;
			$last_value=$record;
			@tmp=split(' ',$record);
                        $record_value=$tmp[0];
			if ($line==1 && $record_value>$max_start){$max_start=$record_value;}
						
		}
		if ($line>1)
		{
			@tmp = split(' ', $last_value);
			$last_time = $tmp[0]; 
			if ($last_time <$min_end)
			{
			$min_end=$last_time;}
		}
		close(CHECKBOOK);
	}
	

}
$runtime = $min_end-$max_start;
$middle=1-$tail-$head;
$r8=(1-$head-$tail)*$runtime;
$fp=$head*$runtime;
$lp=$tail*$runtime;
$startt=$max_start+$fp;
$endt=$min_end-$lp;
print "run time is $runtime, middle is $r8, from $startt to $endt\n";
for ($it=1; $it<=$Rings; $it++)
{
        
	$total_requests=0;
	#create first ring's file

	$file_ring = "$log_file_prefix"."$it".".csv";
	print "Opening $file_ring\n";
	open (RINGFILE, ">$file_ring");
	$Ranks=shift;
	print "=============== Ring $it, $Ranks ranks =============\n";
	if ($Ranks == '')
	{
		print "Rank error....\n";
		exit -1;
	}
	for ($rank=1; $rank<=$Ranks; $rank++)
	{
		#read the ring's rank file first
	 	#from second line, output intervals to the it(th) line of the ring's file 
		$ra = $rank-1;
		$file= "$log_file_prefix"."$it"."_"."$ra";
		#print "$file\n";
		open (CHECKBOOK, "$file");
		$line=0;
		while ($record = <CHECKBOOK>) {
			#print $record;

			@tmp=split(' ',$record);
			$start_value=$tmp[0];
			$end_value=$tmp[6];
       	                $line++;
			$resp = $end_value-$start_value;			
			#print "$end_value - $start_value = $resp\n";
			$resp=$resp*1000;
			print RINGFILE "$start_value $resp\n";
			$total_requests++;

		}
		close(CHECKBOOK);
		
	}
	
	print "total:$total_requests\n";
	close (RINGFILE); 
	
}
