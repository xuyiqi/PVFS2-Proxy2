#!/usr/bin/perl

use List::MoreUtils qw(uniq);

#
#  Checkpointing needs writing to a new file.
#  EXT3 using journaling seems to meet this requirement
#
#  PVFS SERVER CODE NEEDS to be modified to change 256 tcp queue to a much larger value
#

######### Parsing Arguments ###########

#sync after drop_caches

#######################################

$numArgs = $#ARGV + 1;
@paranames = ("APP_FILE","Log Prefix");
#$PerformanceInterval=5;

if ($numArgs < 2)
{
	print "error arguments: @paranames\n";
	exit -1;
}
print "$numArgs command-line arguments:\n";

print "############### Outside Parameters #############\n";

foreach $argnum (0 .. $#ARGV) {
   print "$paranames[$argnum]:\t$ARGV[$argnum]\n";
}

print "############### Embedded Parameters #############\n";

$proxytop="weights.top";
$proxymid="weights.mid";
$proxymida="weights.mid.asym";
$proxybottom="weights.bottom";


$AppFile=$ARGV[0];
$LogPrefix=$ARGV[1];

import_app_params($AppFile);

print "CleanUp:\t@CleanUps\n";
print "NumApp:\t\t$NumApp\n";
print "Iterations:\t$Iterations\n";
print "PvfsConfig:\t@PvfsConfigs\n";
print "TestMode:\t$TestMode\n";
print "AsymNodes:\t@AsymNodes\n";

print "Proxy Top:\t\t$proxytop\n";
print "Proxy Mid(2):\t\t$proxymid, $proxymida\n";
print "Proxy Bottom:\t\t$proxybottom\n";
print "PVFS mnt:\t\t$pvfsmnt\n";
print "PVFS:\t\t\t$pvfslocation\n";
print "MPI:\t\t\t$mpilocation\n";
print "IOR:\t\t\t$iorlocation\n";
print "WRF:\t\t\t$wrflocation\n";
print "NPB:\t\t\t$npblocation\n";
print "LOGs:\t\t\t$statslocation\n";

#output app specific global values for confirmation

print "    Read and Write:\t@app_rw\n";
print "    Application weight:\t@app_wt\n";
print "    Application access:\t@app_access\n";
print "    Application percent:\t@app_percent\n";
print "    Number of Clients:\t@app_client\n";
print "    Machine Files:\t@app_machine\n";
print "    Clients:\t\t@app_clts\n";#might need splitting again.
print "    Servers:\t\t@app_servers\n";#might need splitting again.
print "    File Size:\t\t@app_fsize\n";
print "    Start Time:\t\t@app_startt\n";
print "    Exec Time:\t\t@app_length\n";
print "    File Name:\t\t@app_fname\n";
print "    Buffer Size:\t@app_block\n";

#end output

######### End ##########

$result="cp $AppFile .";
print "$result\n";
$r=`$result`;
print "$r";

#for all apps, prepare their respective config.....
$result="echo \"[apps]\ncount=$NumApp\" > weights.ini";# import apps
print "$result\n";
$r=`$result`;
print "$r";

for ($ap=1; $ap<=$NumApp; $ap++)
{
	$result="echo \"app$ap=$ap\" >> weights.ini";# import apps
	print "$result\n";
	$r=`$result`;
	print "$r";
}

$result="echo \"\n\" >> weights.ini";
print "$result\n";
$r=`$result`;
print "$r";

$result="echo \"[weights]\" >> weights.ini";# import apps
print "$result\n";
$r=`$result`;
print "$r";

for ($ap=1; $ap<=$NumApp; $ap++)
{
	$result="echo \"app$ap=$app_wt[$ap-1]\" >> weights.ini";# import apps
	print "$result\n";
	$r=`$result`;
	print "$r";
}

$result="echo \"[latencies]\" >> weights.ini";# import apps
print "$result\n";
$r=`$result`;
print "$r";

for ($ap=1; $ap<=$NumApp; $ap++)
{
        $result="echo \"app$ap=$app_lt[$ap-1]\" >> weights.ini";# import apps
        print "$result\n";
        $r=`$result`;
        print "$r";
}

$result="echo \"[rates]\" >> weights.ini";# import apps
print "$result\n";
$r=`$result`;
print "$r";

for ($ap=1; $ap<=$NumApp; $ap++)
{
        $result="echo \"app$ap=$app_rate[$ap-1]\" >> weights.ini";# import apps
        print "$result\n";
        $r=`$result`;
        print "$r";
}



$result="cat $Locations";
print "$result\n";
$r=`$result`;
print "$r";

$result="cat $Locations >> weights.ini";
print "$result\n";
$r=`$result`;
print "$r";


$result="echo \"[$Algorithm]\" >> weights.ini";
print "$result\n";
$r=`$result`;
print "$r";

for (my $i=0; $i<$#aps+1; $i++)
{
	$result="echo \"$aps[$i]\" >> weights.ini";
	print "$result\n";
	$r=`$result`;
	print "$r";
}


$result="echo \"\n[proxy]\" >> weights.ini";
print "$result\n";
$r=`$result`;
print "$r";

$result="echo \"performance_interval=$PerformanceInterval\" >> weights.ini";
print "$result\n";
$r=`$result`;
print "$r";



print "\nConfig File Generated:\n\n";

$result="cat weights.ini";
print "$result\n";
$r=`$result`;
print "$r";

#end for apps

print "******************************************************\n";
print "****************Starting main program*****************\n";
print "******************************************************\n";
@appfile=split('/',$AppFile);

$AppFileLast=$appfile[$#appfile];

print "App file\n";
print "$AppFileLast\n";

for ($it=1; $it<=$Iterations; $it++)
{
	$logdir = "$LogPrefix-$AppFileLast-$it";
	$r = "mkdir $softwarelocation/ranks/$logdir";
	print "$r\n";
	
	$result = `$r`;
	print "$result\n";

	$r="export MPIO_DIRECT_READ=TRUE";
 	print "$r\n";
	$result = `$r`;
	print $result;
	$r="export MPIO_DIRECT_WRITE=TRUE";
 	print "$r\n";
	$result = `$r`;
	print $result;
	
	$result=`export`;
	print "$r\n";
	print "Iteration $it\n";
	
	############### clean all clients (IOR2, mpiring, other benchmarks (WRF, NPB))

	for ($l=$NumApp; $l>0; $l--)
	{
		@clts=split(',',$app_clts[$l-1]);
		print "Qutting and Restarting App $l Clients....$app_clts[$l-1], @clts\n";
		#split its client ids from app_clts
		for ($i=0; $i<$#clts+1;$i++){
	                $r = "ssh $ClientNodes[$clts[0]-1] 'uname -a >> $softwarelocation/ranks/$logdir/clientnodes.txt'";
        	        print "$r\n";
                	$result = `$r`;
	                print $result;


			$result = "ssh $ClientNodes[$clts[$i]-1] killall -w IOR mpstat iostat sar vmstat top pvfs2-server";
			print "$result\n";
			$r=`$result`;
			print $r;
			$result = "ssh $ClientNodes[$clts[$i]-1] $mpilocation/mpdallexit";			
			print "$result\n";
			$r=`$result`;
			print $r;
			$r = "ssh $ClientNodes[$clts[$i]-1] sudo /home/users/yiqi/drop_caches";
			print "$r\n";
			$result = `$r`;
			print $result;
			$r = "ssh $ClientNodes[$clts[$i]-1] sync";
			print "$r\n";
			$result = `$r`;
			print $result;

		}
		print "Ring's first node is $ClientNodes[$clts[0]-1].\n";
		$result = "ssh $ClientNodes[$clts[0]-1] $mpilocation/mpd --daemon --ncpus=128";
		print "$result\n";
		$r=`$result`;
		print $r;
		$result2="ssh $ClientNodes[$clts[0]-1] $mpilocation/mpdtrace -l";
		print "$result2\n";
		$r2=`$result2`;
		print $r2;

                if ($vmstat ==1)
                {

			$r = "ssh $ClientNodes[$clts[0]-1] 'vmstat -n 1 > $softwarelocation/ranks/$logdir/vmstat_$ClientNodes[$clts[0]-1].txt &'";
			print "$r\n";
			$result = `$r`;
			print $result;
		}
                if ($sar ==1)
                {
	                $r = "ssh $ClientNodes[$clts[0]-1] 'sar -n DEV -o $softwarelocation/ranks/$logdir/sar_$ClientNodes[$clts[0]-1].raw 1 7200 >/dev/null 2>&1 &'";
                	print "$r\n";
        	        $result = `$r`;
	                print $result;
		}

		#$result2="c1n1_8888 (182.168.122.115)";

		@hostport1=split('_', $r2);#using mpdtrace -l results
		print $hostport1[1]."\n";
		@hostport2=split(' ', $hostport1[1]);
		print "IP:".$hostport1[0].':'.$hostport2[0]."\n";
	
		for ($i=1; $i<$#clts+1;$i++){
	
			$r = "ssh $ClientNodes[$clts[$i]-1] sudo /home/users/yiqi/drop_caches";
			print "$r\n";
			$result = `$r`;
			print $result;

			$r = "ssh $ClientNodes[$clts[$i]-1] sync";
			print "$r\n";
			$result = `$r`;
			print $result;

	                if ($vmstat ==1)
	                {
				$r = "ssh $ClientNodes[$clts[$i]-1] 'vmstat -n 1 > $softwarelocation/ranks/$logdir/vmstat_$ClientNodes[$clts[$i]-1].txt &'";
				print "$r\n";
				$result = `$r`;
				print $result;
			}
	                if ($sar ==1)
        	        {

                	        $r = "ssh $ClientNodes[$clts[$i]-1] 'sar -n DEV -o $softwarelocation/ranks/$logdir/sar_$ClientNodes[$clts[$i]-1].raw 1 7200 >/dev/null 2>&1 &'";
                        	print "$r\n";
	                        $result = `$r`;
        	                print $result;
			}
			$r = "ssh $ClientNodes[$clts[$i]-1] $mpilocation/mpd --daemon --ncpus=128 --host=$hostport1[0] --port=$hostport2[0]";
			print "$r\n";
			$result = `$r`;
			print $result;
		}
		$result="ssh $ClientNodes[$clts[0]-1] $mpilocation/mpdtrace -l";
		print "$result\n";
		$r=`$result`;
		print $r;
	}

	############# Clean the Proxies ######################

	for ($i=0; $i<$#AllServerNodes+1;$i++){
		$r = "ssh $AllServerNodes[$i] 'uname -a >> $softwarelocation/ranks/$logdir/servernodes.txt'";
                print "$r\n";
                $result = `$r`;
                print $result;

		$r = "ssh $AllServerNodes[$i] killall -w proxy2 sar vmstat iostat mpstat top";
		print "$r\n";
		$result = `$r`;
		print $result;
	}

	############### clean all servers (PVFS2)
	# clean storage
	# kill server
	# restart server according to C/S parameters
	#############################################

	@lfs=split(';',$LocalFS);
	$fsinit_raw=$lfs[0];
	@fsinits=split(',',$fsinit_raw);
	$fsinit=$fsinits[$it-1];
	$fslevel=$lfs[1];
	$fstype=$lfs[2];
	$devdir=$lfs[3];
	$fsopt=$lfs[4];
	$fsopt=~ s/\|/=/g;

	print "initraw is $LocalFS,$fsinit_raw\n";
	if ($fsinit == 1)
	{
		print "============ Making FS on the servers ==========\n";
		for (my $fsi=0; $fsi<$#AllServerNodes+1; $fsi++){

			$result= "ssh $AllServerNodes[$fsi] $softwarelocation/mkfs.pl $fslevel $fstype $devdir $PvfsStorage $fsopt";
			print "$result\n";
			$r = `$result`;
			print "$r";
		}
		print "============ End making FS on the servers ==========\n";
	}

	$result="cat server-rc.top > server-rc";
	print "$result\n";
	$r=`$result`;
	print "$r\n";

	$result="echo 'data_servers=( @AllServerNodes )' >> server-rc";
	print "$result\n";
	$r=`$result`;
	print "$r\n";

        $result="echo 'meta_servers=( @MetaServerNodes )' >> server-rc";
        print "$result\n";
        $r=`$result`;
        print "$r\n";


	$result="cat server-rc.bottom >> server-rc";
	print "$result\n";
	$r=`$result`;
	print "$r\n";

	$result="chmod 777 server-rc";
	print "$result\n";
	$r=`$result`;
	print "$r\n";

	$result="cat server-rc";
	print "$result\n";
	$r=`$result`;
	print "$r\n";

	$rc="server-rc";
	$config=$softwarelocation."/temp-config.txt";

	print "Using server config file $config, control file $rc\n";

	if ($confinit==1)
	{ 

		print "Generating PVFS config file automatically....\n";

		$result="echo '$PvfsStorage' > pvfsconf.def";
		print "$result\n";
		$r=`$result`;
		print "$r\n";

		$result="echo '$PvfsStorage/pvfs_log' >> pvfsconf.def";
		print "$result\n";
		$r=`$result`;
		print "$r\n";

		$replacednodes = "@ServerNodes";
		print "$replacednodes\n";
		$replacednodes =~ s/ /,/g;
		$result="echo '$replacednodes' >> pvfsconf.def";
		print "$result\n";
		$r=`$result`;
		print "$r\n";

                $result="echo '$IOMode' >> pvfsconf.def";
                print "$result\n";
                $r=`$result`;
                print "$r\n";
		
                $result="echo 'tcp' >> pvfsconf.def";
                print "$result\n";
                $r=`$result`;
                print "$r\n";

                $result="echo 'no' >> pvfsconf.def";
                print "$result\n";
                $r=`$result`;
                print "$r\n";

                $replacednodes = "@MetaServerNodes";
                print "$replacednodes\n";
                $replacednodes =~ s/ /,/g;
                $result="echo '$replacednodes' >> pvfsconf.def";
                print "$result\n";
                $r=`$result`;
                print "$r\n";

                $result="echo 'y' >> pvfsconf.def";
                print "$result\n";
                $r=`$result`;
                print "$r\n";

                $result="echo 'y' >> pvfsconf.def";
                print "$result\n";
                $r=`$result`;
                print "$r\n";

                $result="echo '$MetaSync' >> pvfsconf.def";
                print "$result\n";
                $r=`$result`;
                print "$r\n";


		###change to software location

		$result="./pvfs2-genconfig pvfsconf.def temp-config.txt --server-job-timeout 300 --client-job-timeout 3000";
		print "$result\n";
		$r=`$result`;
		print "$r\n";
	
		$result="cat temp-config.txt";
		print "$result\n";
		$r=`$result`;
		print "$r\n";

	}
	if ($CleanUps[$it-1]==1)
	{
		print "==================Cleaning up servers===============\n";

		print "$rc $pvfslocation $softwarelocation/$config\n";
		$r = "$softwarelocation/$rc kill_servers $pvfslocation $config";
		print "$r\n";
		$result = `$r`;
		print $result;

		$r = "$softwarelocation/$rc all $pvfslocation $config";
		print "$r\n";
		$result = `$r`;
		print $result;

	}
	elsif ($CleanUps[$it-1]==-1)
	{
		
		print "==================Restarting servers===============\n";
                print "$rc $pvfslocation $softwarelocation/$config\n";
                $r = "$softwarelocation/$rc restart_server $pvfslocation $config";
                print "$r\n";
                $result = `$r`;
                print $result;
	}
	else
	{
		print "==================Read or writing to old file, skipping cleaning up of servers===============\n";
	}

	if ($DropCaches[$it-1]==1)
	{
		print "==================Dropping Caches on servers===============\n";

		for (my $dci=0; $dci<$#AllServerNodes+1; $dci++){
			$r = "ssh $AllServerNodes[$dci] sudo /home/users/yiqi/drop_caches";
			print "$r\n";
			$result = `$r`;
			print $result;

			$r = "ssh $AllServerNodes[$dci] sync";
			print "$r\n";
			$result = `$r`;
			print $result;
		}

	}


	################### No more cleaning below this point ######################

	################### Setup asymmetric server-side files #######################


	
	generate_tab();

	##### prepare for the test dir if test app is MDTEST ##########

        $r="cp $softwarelocation/pvfs2tab.o $homelocation/pvfs2tab";
        print "$r\n";
        $result = `$r`;
        print $result;
        $r="cp $softwarelocation/pvfs2tab.o ./pvfs2tab";
        print "$r\n";
        $result = `$r`;
        print $result;

	#for each app
	for (my $num=1; $num<=$NumApp; $num++)
	{
		print "app $num, $app_names[$num-1]\n";
		if ($app_names[$num-1] eq "MD")
		{
			if ($app_fname[$num-1] eq 'PREFIX')
			{
				$r="$pvfsutilitylocation/pvfs2-mkdir $pvfsmnt/$LogPrefix";
			}
			else
			{
				$r="$pvfsutilitylocation/pvfs2-mkdir $pvfsmnt/$app_fname[$num-1]";
			}
		        print "$r\n";
		        $result = `$r`;
	        	print $result;
		}
		else
		{
			
		}
	}
	if ($TestMode eq "ASYM")#&& $CleanUps[$it-1]==1)
	{

		print "================Setup asymmetric server-side files==============\n";
		$r="cp $softwarelocation/pvfs2tab.o $homelocation/pvfs2tab";
		print "$r\n";
		$result = `$r`;
		print $result;
		$r="cp $softwarelocation/pvfs2tab.o ./pvfs2tab";
		print "$r\n";
		$result = `$r`;
		print $result;

		for (my $num=1; $num<=$NumApp; $num++)
		{
			if ($CleanUps[$it-1]==1)
			{
				$r="$softwarelocation/pvfs2/bin/pvfs2-rm $pvfsmnt/$app_fname[$num-1]";
				print "$r\n";
				#$result=`$r`;
				#print $result;
			}
			if ($app_touchs[$num-1]==1)
			{
				@realasymnodes=split(',',$AsymNodes[$num-1]);
				print "$AsymNodes[$num-1] --- @realasymnodes\n";
				$realasymnode="";
				my $n;
				for ($n=0;$n<$#realasymnodes;$n++)
				{
					$realasymnode=$realasymnode."tcp://".$AllServerNodes[$realasymnodes[$n]-1].":3334,";
				}
				$realasymnode=$realasymnode."tcp://".$AllServerNodes[$realasymnodes[$n]-1].":3334";

				$r="$softwarelocation/pvfs2/bin/pvfs2-touch $pvfsmnt/$app_fname[$num-1] -l $realasymnode";
				print "$r\n";
				$result=`$r`;
				print $result;
			}
		}
	}

	sleep(5);


	############# Start the Proxies ##########################

	$r = "rm $softwarelocation/stat?.txt";
	print "$r\n";
	$result = `$r`;
	print $result;

	if ($Algorithm eq 'NATIVE'){
		print "######### Native Mode Skipping proxy start.... #########\n";

                for ( my $count = 0; $count < $#AllServerNodes+1; $count++) {
	                if ($vmstat ==1)
        	        {
                        	$r = "ssh $AllServerNodes[$count] 'vmstat -n $PerformanceInterval > $softwarelocation/ranks/$logdir/vmstat_$AllServerNodes[$count].txt &'";
                	        print "$r\n";
        	                $result = `$r`;
	                        print $result;
			}
	                if ($sar ==1)
        	        {
	
        	                $r = "ssh $AllServerNodes[$count] 'sar -n DEV -o $softwarelocation/ranks/$logdir/sar_$AllServerNodes[$count].raw $PerformanceInterval 7200 >/dev/null 2>&1 &'";
                	        print "$r\n";
                        	$result = `$r`;
	                        print $result;
			}
			if ($mpstat ==1)
			{
                        	$r = "ssh $AllServerNodes[$count] 'mpstat -P ALL $PerformanceInterval > $softwarelocation/ranks/$logdir/mpstat_$AllServerNodes[$count].txt &'";
                	        print "$r\n";
        	                $result = `$r`;
	                        print $result;
			}
	                if ($iostat ==1)
	                {

        	                $r = "ssh $AllServerNodes[$count] 'iostat $PerformanceInterval > $softwarelocation/ranks/$logdir/iostat_$AllServerNodes[$count].txt &'";
                	        print "$r\n";
                        	$result = `$r`;
	                        print $result;
			}
		}

	}
	else
	{
		for ( my $count = 0; $count < $#AllServerNodes+1; $count++) {
	                if ($vmstat ==1)
        	        {
				$r = "ssh $AllServerNodes[$count] 'vmstat -n $PerformanceInterval > $softwarelocation/ranks/$logdir/vmstat_$AllServerNodes[$count].txt &'";
				print "$r\n";
				$result = `$r`;
				print $result;
			}
	                if ($sar ==1)
	                {
	                        $r = "ssh $AllServerNodes[$count] 'sar -n DEV -o $softwarelocation/ranks/$logdir/sar_$AllServerNodes[$count].raw $PerformanceInterval 7200 >/dev/null 2>&1 &'";
        	                print "$r\n";
                	        $result = `$r`;
                        	print $result;
			}
	                if ($mpstat ==1)
        	        {

                	        $r = "ssh $AllServerNodes[$count] 'mpstat -P ALL $PerformanceInterval > $softwarelocation/ranks/$logdir/mpstat_$AllServerNodes[$count].txt &'";
        	                print "$r\n";
	                        $result = `$r`;
                        	print $result;
			}
	                if ($iostat ==1)
        	        {
	
				$r = "ssh $AllServerNodes[$count] 'iostat $PerformanceInterval > $softwarelocation/ranks/$logdir/iostat_$AllServerNodes[$count].txt &'";
				print "$r\n";
				$result = `$r`;
				print $result;
			}
	        	my $pid = fork();
        		if ($pid) {
		        # parent
        			print "proxy pid is $pid, parent $$\n";
			        push(@childs_proxy, $pid);
	        	} elsif ($pid == 0) {
        		        # child
        	        	sub_proxy($AllServerNodes[$count], $app_wr[$count]);
		                exit 0;
        		} else {
        	        	die "couldnt fork to start proxy: $!\n";
		        }
		}
	}

	####### to do the umount/cleaning up, we need to use the native environment #######
        #no proxy at all
        $r="cp $softwarelocation/pvfs2tab.o $homelocation/pvfs2tab";
        print "$r\n";
        $result = `$r`;
        print $result;

        $r="cp $softwarelocation/pvfs2tab.o ./pvfs2tab";#assume it's in software location launching the script
        print "$r\n";
        $result = `$r`;
        print $result;


	if ($MPIMode eq "real")
	{
		print "###### real mpi mode ########\n";

	}
	else # simulated
	{
		print "######### simulated mpi mode ###########3\n";
		for (my $l=0; $l<$NumApp; $l++)
		{
			my @clts=split(',',$app_clts[$l]);
			for ($i=0; $i<$#clts+1;$i++)
			{	
				#clean up a little bit first...
				$r = "ssh $ClientNodes[$clts[$i]-1] sudo umount $pvfsmnt";
				print "$r\n";
				$result = `$r`;
				print $result;

				$r = "ssh $ClientNodes[$clts[$i]-1] sudo killall -w pvfs2-client";
				print "$r\n";
				$result = `$r`;
				print $result;

				$r = "ssh $ClientNodes[$clts[$i]-1] sudo rmmod pvfs2";
				print "$r\n";
				$result = `$r`;
				print $result;

				#export current_pvfs2tab location
				#setenv PVFS2TAB_FILE /home/username/pvfs2-build/pvfs2tab

				# load kernel module
				$r = "ssh $ClientNodes[$clts[$i]-1] sudo insmod $pvfskolocation/pvfs2.ko";
				print "$r\n";
				$result = `$r`;
				print $result; 

				#start client
				$r = "ssh $ClientNodes[$clts[$i]-1] sudo $pvfsclientlocation/pvfs2-client -p $pvfsclientlocation/pvfs2-client-core";
				print "$r\n";
				$result = `$r`;
				print $result;
			}
		}
	}


        ###### file system virtualization setup ######
        print "###### file system virtualization setup ######\n";


        if ($Algorithm eq 'NATIVE')
        {
                #no proxy at all
                $r="cp $softwarelocation/pvfs2tab.o $homelocation/pvfs2tab";
                print "$r\n";
                $result = `$r`;
                print $result;

                $r="cp $softwarelocation/pvfs2tab.o ./pvfs2tab";#assume it's in software location launching the script
                print "$r\n";
                $result = `$r`;
                print $result;
        }
        else
        {
                $r="cp $softwarelocation/pvfs2tab.k $homelocation/pvfs2tab";
                print "$r\n";
                $result = `$r`;
                print $result;

                $r="cp $softwarelocation/pvfs2tab.k ./pvfs2tab";#assume it's in software location launching the script
                print "$r\n";
                $result = `$r`;
                print $result;
        }



	$r="cp pvfs2tab pvfs2tab.txt";#assume it's in software location launching the script
	print "$r\n";
	$result = `$r`;
	print $result;

	#$dummy=<STDIN>;

	############# end of initialization ##############



	

	sleep(15);#maybe wait for 10 seconds in larger scale and DSFQ mode

	if ($MPIMode eq "real")
	{}
	else
	{
                print "######### simulated mpi mode ###########3\n";
                for (my $l=0; $l<$NumApp; $l++)
                {
                        my @clts=split(',',$app_clts[$l]);
                        for ($i=0; $i<$#clts+1;$i++)
                        {


                                #mount disk drive
                                $baseport=3334;
                                if ($Algorithm eq 'NATIVE')
                                {

                                }
                                else
                                {
                                        $baseport=3335;
                                }
                                $r = "ssh $ClientNodes[$clts[$i]-1] sudo mount -t pvfs2 tcp://$MetaServerNodes[0]:$baseport/pvfs2-fs $pvfsmnt";
                                print "$r\n";
                                $result = `$r`;
                                print $result;

                                $r = "ssh $ClientNodes[$clts[$i]-1] mount | grep pvfs2";
                                print "$r\n";
                                $result = `$r`;
                                print $result;
			}
		}
	}
	print "############# Testing Test Mode ##############\n";
	print "########### Fork Processes for MPI/Benchmarks! ##########\n";

	############### start top ##############
	for ( my $count = 0; $count < $#AllServerNodes+1; $count++) {
        	$r = "ssh $AllServerNodes[$count] ps -e |grep pvfs2";
	        print "$r\n";
	        $result = `$r`;
        	print $result;

	        #split

	        @grep1=split(' ', $result);
	        print "pvfs2: $grep1[0]\n";

	        $r = "ssh $AllServerNodes[$count] ps -e |grep proxy2";
	        print "$r\n";
        	$result = `$r`;
	        print $result;

	        #split

        	@grep2=split(' ', $result);
	        print "proxy2: $grep2[0]\n";
		if ($grep2[0] eq '')
		{
			$grep2[0]='1';
		}
		if ($top ==1)
		{		
        		$r = "ssh $AllServerNodes[$count] 'top -b -d 1 -p $grep1[0],$grep2[0] > $softwarelocation/ranks/$logdir/top_$AllServerNodes[$count].txt &'";
		
	        	print "$r\n";
	        	$result = `$r`;
		        print $result;
		}
#                $r = "ssh $ServerNodes[$count] taskset -p 01 $grep1[0]";
#                print "$r\n";
#                $result = `$r`;
#                print $result;

#                if ($grep2[0] != 1)
#                {
#                        $r = "ssh $ServerNodes[$count] taskset -p 02 $grep2[0]";
#	                print "$r\n";
#        	        $result = `$r`;
#                	print $result;
#                }

#		$grep1[0],$grep2[0]

	        #start top output to rank /logdir folder
	}


        for ( my $count = 1; $count <= $NumApp; $count++) {
                print "fffffffffffffffffffffffffffffffffff $count ffffffffffffffffffffffffffffffffffff\n";
                my $pid = fork();
                if ($pid) {
                        # parent
                        print "benchmark id is $pid, parent $$\n";
                        push(@childs_mpi, $pid);
                } elsif ($pid == 0) {
                        # child
                        sub_mpi($count);
                        exit 0;
                } else {
                        die "couldnt fork: $!\n";
                }
        }

	#$dummy=<STDIN>;

	sleep(5);

	########### Done with Tests, Final Cleaning Up ################

	print "################################### Waiting for proxy child... ####################################\n";

	## try to kill the other ring(s)

	####### total test time elapsed....

	if ($TestTime eq "WAIT")
	{
                $sleepid = fork();
                if ($sleepid) {
                        # parent
			my $childs=$#childs_mpi+1;
	                print "^^^^^^^^^^^^ This test will wait for $childs benmchmark instances to finish ^^^^^^^^^^^^\n";
        	        foreach (@childs_mpi) {
                	        my $tmp = waitpid($_, 0);
                        	print "done with mpi pid $tmp\n";
                	}
			print "killing sleep thread - init is $Initialize\n";
			system ("kill $sleepid");
		        if ($Initialize==1)
        		{
                		print "########## [fork] Initialize only tag found, exiting before anything else happens... ###########\n";
                		exit(0);
        		}

                } elsif ($sleepid == 0) {
                        # child
                        sub_timer();
			exit 0;
                        
                } else {
                        die "couldnt fork: $!\n";
                }

	}
	elsif ($TestTime eq 'WAITFASTEST')
	{

                $sleepid = fork();
                if ($sleepid) {
                        # parent
                } elsif ($sleepid == 0) {
                        # child
                        sub_timer();
			exit 0;
                        
                } else {
                        die "couldnt fork: $!\n";
                }
                print "^^^^^^^^^^^^ This test will wait for the FASTEST benmchmark instance to finish ^^^^^^^^^^^^\n";
                foreach (@childs_mpi) {
                        my $tmp = waitpid($_, 0);
                        print "done with mpi pid $tmp\n";
                }
	        if ($Initialize==1)
        	{
                	print "########## [fork] Initialize only tag found, exiting before anything else happens... ###########\n";
               		 exit(0);
        	}
	}
	else
	{
		print "^^^^^^^^^^^^ This test will last at least $TestTime seconds ^^^^^^^^^^^^\n";
		my $ttlsleep=0;
		while ($ttlsleep<$TestTime)
		{
			sleep(1);
			$ttlsleep++;
			if ($LifetimeCounter > 0 && $ttlsleep % $LifetimeCounter == 0)
			{
				print "[total test] $ttlsleep\n";
			}
		}
	
		####### let assume that all programs are started by mpi first. ######
		####### because sub_mpi calls subsequent multi forks, killing it will kill separate cp processes too, hopefully #######

		print "done with child process for MPIs...killing the ring\n";
		#Use form() again
		for ( my $count = 1; $count <= $NumApp; $count++) {
			my $pid = fork();
			if ($pid) {
				# parent
				print "pid is $pid, parent $$\n";
				push(@childs_mpi_kill, $pid);
			} elsif ($pid == 0) {
				# child
				sub_kill_mpi($count);
				exit 0;
			} else {
				die "couldnt fork: $!\n";
			}
		}
		
		foreach (@childs_mpi_kill) {
        		my $tmp = waitpid($_, 0);
			print "done with mpi kill pid $tmp\n";
		}
	}
		
	for ($s=1; $s<=$#AllServerNodes+1; $s++)
	{
		$r= "ssh $AllServerNodes[$s-1] killall -w proxy2 sar vmstat mpstat iostat top";
		print "$r\n";
		$result = `$r`;
		print $result;
	}

	foreach (@childs_proxy) {
        	my $tmp = waitpid($_, 0);
	         print "done with proxy pid $tmp\n";
	}

	kill($sleepid,2);


	############## Data PostProcessing ###############

	print "############## Data PostProcessing ###############\n";

	#preparing to merge the rank
	$r = "cp ~/log_* .";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	for (my $m=0;$m<$app_client[0];$m++)
	{
	        $r = "cp ~/bt.$m.log .";
        	print "$r\n";
	        $result = `$r`;
        	print "$result\n";

	        $r = "perl ./btio_resp.pl bt.$m.log";
	        print "$r\n";
	        $result = `$r`;
        	print "$result\n";
	}

        $r = "perl ./merge_rank2.pl $NumApp 0.1 0.1 @app_client";
        print "$r\n";
        $result = `$r`;
        print "$result\n";


	for ($j=1; $j<=$NumApp; $j++)
	{
	        $r = "perl ./merge_log.pl $j $app_client[$j-1]";
        	print "$r\n";
	        $result = `$r`;
        	print "$result\n";

                $r = "cat log.$j | sed -e '/-/d' > log.$j.p";
                print "$r\n";
                $result = `$r`;
                print "$result\n";

                $r = "sort log.$j.p > sorted.$j";
                print "$r\n";
                $result = `$r`;
                print "$result\n";

		$r = "awk '{printf(\"%.0f %i\\n\", \$1, \$2) }' sorted.$j > second.$j";
                print "$r\n";
                $result = `$r`;
                print "$result\n";
                $r = "perl ./get_trace.pl second.$j 1 count";
                print "$r\n";
                $result = `$r`;
                print "$result\n";
		
                $r = "sort log_file_rank_$j.csv |sed -e '/-/d' > r$j";
                print "$r\n";
                $result = `$r`;
                print "$result\n";

                $r = "awk '{printf(\"%.0f %i\\n\", \$1, \$2) }' r$j > r$j.s";
                print "$r\n";
                $result = `$r`;
                print "$result\n";

                $r = "sort r$j.s -k 1,1n -k 2,2n > r$j.s2";
                print "$r\n";
                $result = `$r`;
                print "$result\n";

                $r = "./get_trace.pl r$j.s2 1 percentile 0.95";
                print "$r\n";
                $result = `$r`;
                print "$result\n";

                $r = "./get_trace.pl r$j.s2 1 average";
                print "$r\n";
                $result = `$r`;
                print "$result\n";
	
	}

	# right now we have log.1, log.2, ....


        $r = "cp *.log log* r*.s* *trace* sorted* *.resp second.* $softwarelocation/ranks/$logdir";
        print "$r\n";
        $result = `$r`;
        print "$result\n";

	parse_top();

	$r = "cp ~/log_* $softwarelocation/ranks/$logdir";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	#$config
	$r = "cp $config $softwarelocation/ranks/$logdir";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	#$vmstat X
	#top X
	#top process X

	#$rc
	$r = "cp $softwarelocation/$rc $softwarelocation/ranks/$logdir";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	#pvfs2tab.o, pvfs2tab.k pvfs2tab
	$r = "cp pvfs2tab pvfs2tab.o pvfs2tab.k $softwarelocation/ranks/$logdir";
	print "$r\n";
	$result = `$r`;
	print "$result\n";


	#test2.txt, $AppFile X

	#@app_machines

	for (my $mi=0; $mi< $#app_machines; $mi++){
		$r = "cp $mf $softwarelocation/ranks/$logdir/";
		print "$r\n";
		$result = `$r`;
		print "$result\n";
	}

	#weights.ini 	#temp-config
	$r = "cp weights.ini temp-config.txt $softwarelocation/ranks/$logdir";
	print "$r\n";
	$result = `$r`;
	print "$result\n";	

	$r = "cp $AppFile $softwarelocation/ranks/$logdir";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	$r = "cp *.csv $softwarelocation/ranks/$logdir";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	$r = "cp ~/*depthtrack.txt ~/*latency* $softwarelocation/ranks/$logdir";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	$r = "rm ~/*depthtrack.txt ~/*latency*";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	
        $r = "rm ~/log_* ./log_*";
        print "$r\n";
        $result = `$r`;
        print "$result\n";


	$r = "tar -cvzf $logdir".".tgz $softwarelocation/ranks/$logdir/*";
	print "$r\n";
	$result = `$r`;
	print "$result\n";

	############ wrap up everything, including generated config files ##########


} #end iteration

print "=======================End of test program [v9.0]======================\n\n";

exit 0;

sub sub_timer {

	if ($LifetimeCounter==0)
	{
		return;
	}
	my $seconds;
	while (1)
	{
		$seconds=$seconds+$LifetimeCounter;
		sleep($LifetimeCounter);
		print "[total test] $seconds\n";
	}

}

sub sub_proxy {
        $num = shift;
	$method = shift;
        print "started child process for Server $num\n";

	if ($Algorithm eq 'NONE')
	{
		$Algorithm = "none";		
	}

	if ($Performance==1)#count immediately because there is some server which gets only one app's data
	{
		$r = "ssh $num $proxylocation/proxy2 --cost_model=$Cost --buffer=$pbuffer -c$softwarelocation/weights.ini -s$Algorithm --stat=$softwarelocation/ranks/$logdir/stat_$num.txt  --proxy_config=$config -a --log_prefix=$num.proxy";
	}
	else
	{
		$r = "ssh $num $proxylocation/proxy2 --cost_model=$Cost --buffer=$pbuffer -c$softwarelocation/weights.ini -s$Algorithm --stat=$softwarelocation/ranks/$logdir/stat_$num.txt  --proxy_config=$config --log_prefix=$num.proxy";
	}

	if ($PeriodPerformance ==1)
	{
		$r = $r." --periodical_counter";
	}


	print "$r\n";
	if ($Initialize==1)
	{

	}
	else
	{	
		$result = `$r`;
		print "$result";
	}
	
        print "done with child process of proxies on server $num\n";
	return $num;
}


sub sub_mpi {
        $num = shift;
        print "started child process for mpi application $num\n";

	print "ring nodes: $app_clts[$num-1]\n";
	@appmachines=split(',',$app_clts[$num-1]);
	$appmachine=$appmachines[0];
	#I'll wait for another extra few seconds -- demonstrating dynamic capacity allocation
	print "^^^^^^^^^^ application $num is waiting for $app_startt[$num-1] seconds before running ^^^^^^^^^^\n";
	sleep($app_startt[$num-1]);
	$apprun=$app_names[$num-1];
	
	init_benchmark ($num, $apprun);

}

sub sub_kill_mpi {
        $num = shift;
	my @clts = $app_clts[$num-1];
	my $node = $ClientNodes[$clts[0]-1];
	$r = "ssh $node killall -w IOR";
	print "$r\n";
	$result = `$r`;
	print $result;		
}

sub import_app_params {

	$appfile=shift;

	open (CHECKBOOK, $appfile);
	print "app file is $appfile\n";
	$line=0;

	$record=<CHECKBOOK>;
	$Initialize=split_value('=', $record, 1);
	print "Initialize is $Initialize\n";

	$record=<CHECKBOOK>;
	$LifetimeCounter=split_value('=', $record, 1);
        print "LifetimeCounter is $LifetimeCounter\n";	

	$record=<CHECKBOOK>;
	$MPIMode=split_value('=', $record, 1);
	print "MPIMode is $MPIMode\n";

	$record=<CHECKBOOK>;
	$Algorithm=split_value('=', $record, 1);
	print "Algorithm is $Algorithm\n";

	$record=<CHECKBOOK>;
	$AlgorithmParams=split_value('=', $record, 1);
	print "Algorithm Parameters are $AlgorithmParams\n";

	@aps=split(';',$AlgorithmParams);
	for (my $i=0; $i< $#aps+1; $i++)
	{
		$aps[$i]=~ s/\|/=/g;
		print "$i param is $aps[$i]";
	}

	#nothing uses $Servers
	$record=<CHECKBOOK>;
	$Servers=split_value('=', $record, 1);
	print "Servers is $Servers\n";

	$record=<CHECKBOOK>;
	$TestMode=split_value('=', $record, 1);
	print "TestMode is $TestMode\n";
	
	$record=<CHECKBOOK>;
	$Iterations=split_value('=', $record, 1);
	print "Iterations is $Iterations\n";

	$record=<CHECKBOOK>;
	$CleanUp=split_value('=', $record, 1);
	print "CleanUp is $CleanUp\n";
	@CleanUps = split(',',$CleanUp);

	$record=<CHECKBOOK>;
	$DropCache=split_value('=', $record, 1);
	print "DropCache is $DropCache\n";
	@DropCaches = split(',',$DropCache);

	$record=<CHECKBOOK>;
	$PvfsStorage=split_value('=', $record, 1);
	print "PvfsStorage is $PvfsStorage\n";

        $record=<CHECKBOOK>;
        $IOMode=split_value('=', $record, 1);
        print "IOMode is $IOMode\n";

        $record=<CHECKBOOK>;
        $MetaSync=split_value('=', $record, 1);
        print "MetaSync is $MetaSync\n";

	$record=<CHECKBOOK>;
	$LocalFS=split_value('=', $record, 1);
	print "LocalFS is $LocalFS\n";

	$record=<CHECKBOOK>;
	$confinit=split_value('=', $record, 1);
	print "confinit is $confinit\n";

	$record=<CHECKBOOK>;
	$AsymNode=split_value('=', $record, 1);
	print "AsymNode is $AsymNode\n";
	@AsymNodes = split(';',$AsymNode);

	$record=<CHECKBOOK>;
	$ClientNode=split_value('=', $record, 1);
	print "ClientNode is $ClientNode\n";
	@ClientNodes = split(',',$ClientNode);

	$record=<CHECKBOOK>;
	$ServerNode=split_value('=', $record, 1);
	@ServerNodes = split(',',$ServerNode);
	print "ServerNode is $ServerNode\n";

        $record=<CHECKBOOK>;
        $MetaServerNode=split_value('=', $record, 1);
        @MetaServerNodes = split(',',$MetaServerNode);
        print "MetaServerNode is $MetaServerNode\n";

	@AllServerNodesD = (@ServerNodes, @MetaServerNodes);
	@AllServerNodes = uniq(@AllServerNodesD);
        print "AllServerNode is @AllServerNodes\n";

	$record=<CHECKBOOK>;
	$Locations=split_value('=', $record, 1);
	print "Locations is $Locations\n";

	$record=<CHECKBOOK>;
	$homelocation=split_value('=', $record, 1);
	print "homelocation is $homelocation\n";

	$record=<CHECKBOOK>;
	$statslocation=split_value('=', $record, 1);
	print "statslocation is $statslocation\n";

	$record=<CHECKBOOK>;
	$softwarelocation=split_value('=', $record, 1);
	print "softwarelocation is $softwarelocation\n";

	$record=<CHECKBOOK>;
	$pvfsmnt=split_value('=', $record, 1);
	print "pvfsmnt is $pvfsmnt\n";

	$record=<CHECKBOOK>;
	$pvfslocation=split_value('=', $record, 1);
	print "pvfslocation is $pvfslocation\n";

	$record=<CHECKBOOK>;
	$pvfskolocation=split_value('=', $record, 1);
	print "pvfskolocation is $pvfskolocation\n";

	$record=<CHECKBOOK>;
	$pvfsclientlocation=split_value('=', $record, 1);
	print "pvfsclientlocation is $pvfsclientlocation\n";

	$record=<CHECKBOOK>;
	$proxylocation=split_value('=', $record, 1);
	print "proxylocation is $proxylocation\n";

	$record=<CHECKBOOK>;
	$mpilocation=split_value('=', $record, 1);
	print "mpilocation is $mpilocation\n";

	$record=<CHECKBOOK>;
	$iorlocation=split_value('=', $record, 1);
	print "iorlocation is $iorlocation\n";

	$record=<CHECKBOOK>;
	$wrflocation=split_value('=', $record, 1);
	print "wrflocation is $wrflocation\n";

	$record=<CHECKBOOK>;
	$npblocation=split_value('=', $record, 1);
	print "npblocation is $npblocation\n";

        $record=<CHECKBOOK>;
        $mdlocation=split_value('=', $record, 1);
        print "mdlocation is $mdlocation\n";

        $record=<CHECKBOOK>;
        $pvfsutilitylocation=split_value('=', $record, 1);
        print "pvfsutilitylocation is $pvfsutilitylocation\n";

	$record=<CHECKBOOK>;
	$top=split_value('=', $record, 1);
	print "Top is $top\n";

        $record=<CHECKBOOK>;
        $iostat=split_value('=', $record, 1);
        print "IOstat is $iostat\n";

        $record=<CHECKBOOK>;
        $vmstat=split_value('=', $record, 1);
        print "VMstat is $vmstat\n";

        $record=<CHECKBOOK>;
        $mpstat=split_value('=', $record, 1);
        print "MPstat is $mpstat\n";

        $record=<CHECKBOOK>;
        $sar=split_value('=', $record, 1);
        print "Sar is $sar\n";

        $record=<CHECKBOOK>;
        $TestTime=split_value('=', $record, 1);
        print "TestTime is $TestTime\n";

	$record=<CHECKBOOK>;
	$Performance=split_value('=', $record, 1);
	print "Performance is $Performance\n";

	$record=<CHECKBOOK>;
	$PeriodPerformance=split_value('=', $record, 1);
	print "PeriodPerformance is $PeriodPerformance\n";

	$record=<CHECKBOOK>;
	$PerformanceInterval=split_value('=', $record, 1);
	print "PerformanceInterval is $PerformanceInterval\n";

	$record=<CHECKBOOK>;
	$pbuffer=split_value('=', $record, 1);
	print "Proxy buffer is $pbuffer\n";

	$record=<CHECKBOOK>;
	$Cost=split_value('=', $record, 1);
	print "Cost is $Cost\n";

	$record=<CHECKBOOK>;
	$NumApp=split_value('=', $record, 1);
	print "NumApp is $NumApp\n";

	$app=0;	

	open (WEIGHTS,">$Locations");
	while ($record = <CHECKBOOK> ) {
		$app = $app + 1;
		if ($app > $NumApp){last;}
		print "App $app: $record";

		print WEIGHTS "\n[app$app"."_locations]\n";

		$app_name=split_value('=', $record, 1);
		push(@app_names, $app_name);
		print "    Client Application:\t$app_name\n";

		$record = <CHECKBOOK>;
		$rw=split_value('=', $record, 1);
		push(@app_rw, $rw);
		print "    Read and Write:\t$rw\n";

		$record=<CHECKBOOK>;
		$acc=split_value('=', $record, 1);
		push(@app_access, $acc);
		print "    Application access:\t$acc\n";

		$record=<CHECKBOOK>;
		$perc=split_value('=', $record, 1);
		push(@app_percent, $perc);
		print "    Application percent:\t$perc\n";

		$record=<CHECKBOOK>;
		$wt=split_value('=', $record, 1);
		push(@app_wt, $wt);
		print "    Application weight:\t$wt\n";

                $record=<CHECKBOOK>;
                $lt=split_value('=', $record, 1);
                push(@app_lt, $lt);
                print "    Application latency:\t$lt\n";

                $record=<CHECKBOOK>;
                $rate=split_value('=', $record, 1);
                push(@app_rate, $rate);
                print "    Application rate:\t$rate\n";


		$record=<CHECKBOOK>;
		$clts=split_value('=', $record, 1);
		push(@app_client, $clts);
		print "    Number of Clients:\t$clts\n";

		$record=<CHECKBOOK>;
		$mf=split_value('=', $record, 1);
		push(@app_machine, $mf);
		print "    Machine Files:\t$mf\n";

		$record=<CHECKBOOK>;
		$clts=split_value('=', $record, 1);
		push(@app_clts, $clts);
		print "    Clients:\t\t$clts\n";

		#generate machine file for this app! 128 each. in round robin
		@tclts=split(',',$clts);
		
		#split its client ids from app_clts
		open (MACHINEFILE,">$mf");
		for (my $i=0; $i<1024;$i++){
			my $thisi=$i % ($#tclts+1);
			#print "thisi is $thisi, client number is $tclts[$thisi]-1\n";
			print MACHINEFILE "$ClientNodes[$tclts[$thisi]-1]\n";
			#print "$ClientNodes[$tclts[$thisi]-1]\n";
		}
		close (MACHINEFILE);

		$record=<CHECKBOOK>;
		$svs=split_value('=', $record, 1);
		push(@app_servers, $svs);
		print "    Servers:\t\t$svs\n";

		$record=<CHECKBOOK>;
		$fsize=split_value('=', $record, 1);
		push(@app_fsize, $fsize);
		print "    File Size:\t\t$fsize\n";

		$record=<CHECKBOOK>;
		$startt=split_value('=', $record, 1);
		push(@app_startt, $startt);
		print "    Start Time:\t\t$startt\n";

		$record=<CHECKBOOK>;
		$endt=split_value('=', $record, 1);
		push(@app_length, $endt);
		print "    Exec Time:\t\t$endt\n";

		$record=<CHECKBOOK>;
		$fname=split_value('=', $record, 1);
		push(@app_fname, $fname);
		print "    FileName:\t\t$fname\n";

		$record=<CHECKBOOK>;
		$bsize=split_value('=', $record, 1);
		push(@app_block, $bsize);
		print "    Buffer Size:\t$bsize\n";

		$record=<CHECKBOOK>;
		$lcount=split_value('=', $record, 1);
		print "    LCount:\t$lcount\n";
		print WEIGHTS "count=$lcount\n";

		for (my $li=1; $li<$lcount+1; $li++){
			$record=<CHECKBOOK>;
			$lvalue=split_value('=', $record,1);
			print "    Location$li:\t$lvalue\n";
			print WEIGHTS "location$li=$lvalue\n";
	
		}

		print WEIGHTS "\n";
		

		$record=<CHECKBOOK>;
		$param=split_value('=', $record, 1);
		push(@app_touchs, $param);
		print "    Touch is:\t$param\n";

                $record=<CHECKBOOK>;
                $param=split_value('=', $record, 1);
                push(@app_traces, $param);
                print "    Trace is:\t$param\n";

		$record=<CHECKBOOK>;
		$param=split_value('=', $record, 1);
		push(@app_other_param, $param);
		print "    Other Param:\t$param\n";

		$record = <CHECKBOOK>;#absorb the splitting =
		print "splitter $record\n";
		next;
	}
	close(WEIGHTS);
	close(CHECKBOOK);
	#print RINGFILE "\n";
}

sub split_value {
	$sp=shift;
	$from=shift;
	$index=shift;

	chomp($from);
	@i_array=split($sp,$from);
	$result=$i_array[$index];
}

sub init_benchmark {
	my $num = shift;
	my $bench = shift;

	#run it as another process...so that we have a timed kill thereafter...
	my $wait = $app_length[$num-1];
	if ($TestTime eq "WAIT")
	{
		print "^^^^^^^^^ app $num will be allowed to execute until finishing ^^^^^^^^^\n";
		exec_benchmark($num, $bench, "WAIT");
		
	}
	elsif ($TestTime eq 'WAITFASTEST')
	{
		print "^^^^^^^^^ app $num will be allowed to execute until finishing THEN kill others ^^^^^^^^^\n";
		exec_benchmark($num, $bench, "WAITFASTEST");

	}
	else
	{
		print "^^^^^^^^^ app $num will be allowed to execute for $wait seconds ^^^^^^^^^\n";
		exec_benchmark($num, $bench, $wait);
	}

}

sub exec_benchmark{
	my $num = shift;
	my $bench = shift;
	my $wait = shift;


	my $pid = fork();
    if ($pid) {
		# parent
	        print "pid is $pid, parent $$\n";
		push(@childs_bench, $pid);
		
		if ($wait eq "WAIT")
		{
			print "Parent is waiting for all the children...$pid\n";
			my $tmp = waitpid($pid, 0);
			print "done with children pid $tmp\n";
			# init is aware of the wait. waiting for children here.  let the parent wait too.
			
		}
		elsif ($wait eq "WAITFASTEST")
		{
                        print "Parent is waiting for the FASTEST child...$pid\n";
                        my $tmp = waitpid($pid, 0);
                        print "done with children pid $tmp\n";
                        # init is aware of the wait. waiting for children here.  let the parent wait
	                for ( my $count = 1; $count <= $NumApp; $count++) {
                       	        sub_kill_mpi($count);
			}
		}
		else
		{
	                my $mysleep=0;
	                while ($mysleep<$wait)
	                {
        	                sleep(1);
                	        $mysleep++;
	                        if ($mysleep % 5 ==0)
	                        {
        	                        print "[test $num] $mysleep\n";
                	        }
	                }
			#sleep($wait);
			sub_kill_mpi($num);
		}

    } elsif ($pid == 0) {
        # child
		if ($bench eq 'IOR')
		{
			exec_IOR($num, $wait);
		}
		elsif ($bench eq 'WRF')
		{
			exec_WRF($num, $wait)
		}
		elsif ($bench eq 'NAS')
		{
			exec_NAS($num, $wait);
		}
		elsif ($bench eq 'MPIBLAST')
		{
			exec_NAS($num, $wait);
		}
		elsif ($bench eq 'MD')
		{
			exec_MD($num, $wait);
		}
		else
		{
			print "What's $bench?\n";
			exec_OTHER($bench, $wait);
		}
		if ($wait eq "WAIT")
		{
			print "^^^^^^^^^^^ Execution $bench $num Finished...Exiting ^^^^^^^^^^^\n";	
		}
		else
		{
			sleep($wait);#later wait for the parent to kill
		        print "^^^^^^^^^^^ Execution $bench $num Expired...Exiting ^^^^^^^^^^^\n";
		}
	

	
    } else {
               	die "couldnt fork to start $num benchmark: $! $app_names[$num-1]\n";
	}

}
sub exec_MD{
	print "%%%%%%%%%%%%%% Running MDTEST... %%%%%%%%%%%%%%%\n";

        my $num=shift;

        my @clts=split(',',$app_clts[$num-1]);

        my $appmachine = $ClientNodes[$clts[0]-1];

        my $appfn = $app_fname[$num-1];#the target folder name like mdtest18, and is translated to /home/users/yiqi/mnt/mdtest18

        my $changedfns;

        my $i=0;
	if ($appfn eq 'PREFIX')
	{
		$appfn=$LogPrefix;
	}
	else
	{
		
	}
   #     if ($MPIMode eq "real")
   #     {
   #             $changedfns="pvfs2:".$pvfsmnt."/".$appfn;
   #     }
   #     else
   #     {
                $changedfns=$pvfsmnt."/".$appfn;
   #     }


	#./multi-md-test -d /home/users/yiqi/mnt/mdtest18 -i -n 800 -s 65536 -a 1 -p 1 -c 48 -i

        $r = "ssh $appmachine $mpilocation/mpiexec -machinefile $softwarelocation/$app_machine[$num-1] -n $app_client[$num-1] $mdlocation/multi-md-test";
        $r = $r." -d ".$changedfns." -i -n ".$app_block[$num-1]." -s ".$app_fsize[$num-1];
        if ($MPIMode eq 'real')
        {
                $r = $r." -a 1 ";#pvfs
        }
        else
        {
                $r = $r." -a 0 ";#vfs
        }
        $r = $r." -p 1 -c ".$app_client[$num-1];

        $r = $r." ".$app_other_param[$num-1];

        print "$r\n";
        if ($Initialize==1)
        {

                print "########## [IOR] Initialize only tag found, exiting before anything else happens... ###########\n";
                exit(0);
        }

        $result = `$r`;
        print $result;


}

sub exec_IOR{

	print "%%%%%%%%%%%%%% Running IOR... %%%%%%%%%%%%%%%\n";

	my $num=shift;

	my @clts=split(',',$app_clts[$num-1]);

	my $appmachine = $ClientNodes[$clts[0]-1];

	my $appfn = $app_fname[$num-1];

	#split appfn with @;
	#create a new string with @+leading path
	#print it	
	
	my @appfns=split(/@/, $appfn);

	my $changedfns;

	my $i=0;
	if ($MPIMode eq "real")
	{	
		$changedfns="pvfs2:".$pvfsmnt."/".$appfns[0];
	}
	else
	{
		$changedfns=$pvfsmnt."/".$appfns[0];
	}
	
	print "$appfn\n";
	print "$appfns[0]\n";
	for ($i=1; $i< $#appfns+1; $i++)
	{
		print "$appfns[$i]\n";
		$changedfns = $changedfns."@"."pvfs2:".$pvfsmnt."/".$appfns[$i];
	}
	print "$changedfns\n";

	$r = "ssh $appmachine $mpilocation/mpiexec -machinefile $softwarelocation/$app_machine[$num-1] -n $app_client[$num-1] $iorlocation/IOR";
        $r = $r." -L $num\,$app_traces[$num-1] -a ";
	if ($MPIMode eq 'real')
	{
		$r = $r."MPIIO";
	} 
	else
	{
		$r = $r."POSIX";
	}
	$r = $r." -$app_rw[$num-1] -t $app_block[$num-1] -b $app_fsize[$num-1] -vv -o $changedfns";
        
	if ($app_access[$num-1] eq 'randomoffset')
	{
		$r = $r." -z";
	}

	if ($app_access[$num-1] eq 'randomwroffset')
	{
		$r = $r." -z -y -M $app_percent[$num-1]";
	}
	
	$r = $r ." ".$app_other_param[$num-1];

	print "$r\n";
	if ($Initialize==1)
	{

                print "########## [IOR] Initialize only tag found, exiting before anything else happens... ###########\n";
                exit(0);
	}
	
	$result = `$r`;
	print $result;
}

sub exec_cp{

	# this is for one "ring/app" only!
	print "Running cp...\n";
	#use different target file name in this case for each client!
	
	my $num=shift;
	#need to ssh to all the client simultaneously....
	# use process forking again.

	my @clts=split(',',$app_clts[$num-1]);

	$nprocesses=$no_clients[$num]/$#clts;

	print "############ $nprocesses per client node ###########\n";
	for ($i=0; $i<$#clts+1;$i++)
	{
		#n processes...	
		for ($j=0; $j< $nprocesses; $j++)
		{
			my $pid = fork();
			if ($pid) {
				# parent
				print "pid is $pid, parent $$\n";
				push(@childs_cp, $pid);
			} elsif ($pid == 0) {
				# child
				sub_cp($ClientNodes[$clts[$i]-1], $num, $j);
				exit 0;
			} else {
				die "couldnt fork: $!\n";
			}
		}
	}
	# on each client
		# divide the number of clients processes by the number of nodes. each client node runs that many processes
		# start n ssh in parallel


}

sub cp{
	#clean up a little bit first...
	#deprecated for current architecture!
	$nodename=shift;$ring=shift;$proc=shift;
	$r = "ssh $nodename cp /data/yiqidatafile $pvfsmnt/datafile.r$ring.p$proc";
	print "$r\n";
	$result = `$r`;
	print $result;
	# how the file system is loaded is dependent on mpi_mode=real or simulated
	
}

#to be extended, using other_params in the test file
sub exec_WRF{
	print "Running WRF...\n";

	# where does the output go?
	# needs to stand in WRF folder... copy all the file first...before mounting using proxy

	# remember, WRF is also in MPI mode, so not every client ring node needs to run a process manually
	# this means the mpi ring also needs to be created, and that file system is created that way too
}

#to be extended
sub exec_NAS{
	print "Running NAS...\n";
	# where does the output go?

	my $num=shift;

	my @clts=split(',',$app_clts[$num-1]);
	my $appmachine = $ClientNodes[$clts[0]-1];
	$r = "ssh $appmachine $mpilocation/mpiexec -machinefile $softwarelocation/$app_machine[$num-1] -n $app_client[$num-1] $npblocation/$app_fname[$num-1]";
        

	print "$r\n";
	if ($Initialize==1)
	{
		print "########## [NAS] Initialize only tag found, exiting before anything else happens... ###########\n";
		return;
	}
	
	$result = `$r`;
	print $result;	
	
}

#to be extended
sub exec_MPIBLAST{
	print "Running MPIBLAST...\n";
	# where does the output go?
}

sub exec_OTHER{
	my $bench=shift;
	print "Running $bench...?!@#\n";}
sub prep_content{
	#cp WRF/executables into $pvfsmnt

}

sub generate_tab{

	$firstsnode=$MetaServerNodes[0];
	
	$result="tcp://$firstsnode:3334/pvfs2-fs $pvfsmnt pvfs2 defaults,noauto 0 0";
	$result="echo '$result' > pvfs2tab.o";
	print "$result\n";
	$r=`$result`;
	print "$r";

	$result="tcp://$firstsnode:3335/pvfs2-fs $pvfsmnt pvfs2 defaults,noauto 0 0";
	$result="echo '$result' > pvfs2tab.k";
	print "$result\n";
	$r=`$result`;
	print "$r";

}

sub parse_top{
	for (my $topi=0; $topi<$#AllServerNodes+1; $topi++) {

		my $filename = "$softwarelocation/ranks/$logdir/top_$AllServerNodes[$topi].txt";
		print "Processing top_$AllServerNodes[$topi].txt\n";

		$r = "cat $filename |grep yiqi|grep proxy2 > $filename.proxy";
		print "$r\n";
		$result = `$r`;
		print "$result\n";

		$r = "cat $filename |grep yiqi|grep server > $filename.pvfs";
		print "$r\n";
		$result = `$r`;
		print "$result\n";

		#### PROCESS PROXY DATA ####
		print "#### PROCESS PROXY DATA ####\n";

		open (TOP, "$filename.proxy");
		open (OUTTOPCPU, ">$filename.proxy.cpu");
		open (OUTTOPMEM, ">$filename.proxy.mem");

		$line=0;

		while ($record=<TOP>){

			@mems=split(' ',$record);
			$cpu=$mems[8];
			$mem=$mems[9];
	
			#print "cpu is $cpu, mem is $mem from @mems\n";
			print OUTTOPCPU "$cpu\n";
			print OUTTOPMEM "$mem\n";

			#write to the file
		
		}
		close (TOP);
		close (OUTTOPCPU);
		close (OUTTOPMEM);

		#### PROCESS SERVER DATA ####
		print "#### PROCESS SERVER DATA ####\n";

		open (TOP, "$filename.pvfs");
		open (OUTTOPCPU, ">$filename.pvfs.cpu");
		open (OUTTOPMEM, ">$filename.pvfs.mem");

		$line=0;

		while ($record=<TOP>){

			@mems=split(' ',$record);
			$cpu=$mems[8];
			$mem=$mems[9];
	
			#print "cpu is $cpu, mem is $mem from @mems\n";
			print OUTTOPCPU "$cpu\n";
			print OUTTOPMEM "$mem\n";

			#write to the file
		
		}
		close (TOP);
		close (OUTTOPCPU);
		close (OUTTOPMEM);

	}
}

sub load_pvfs_client
{
	#load client.
	#load kernel module
	#mount
}

sub uniq_my {
	my %seen = ();
	my @r = ();
	foreach my $a (@_) {
		unless ($seen{$a}) {
			push @r, $a;
			$seen{$a} =1;
		}
	}

}

sub unload_pvfs_client
{
	#unload client.
	#unload kernel module
	#unmount
}
