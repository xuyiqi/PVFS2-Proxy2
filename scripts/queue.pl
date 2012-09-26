#!/usr/bin/perl


######### Parsing Arguments ###########

	print "Opening\n";
	open (CHECKBOOK, $ARGV[0]);
	$lineval="";
	$line=-1;
	open (OUTPUT, ">ratio.txt");
		while ($record = <CHECKBOOK>) {
			#print "$record\n";			
			if ($lineval ne $record)
			{
			
				if ($line==-1)
				{
					print "starting with $record - ";
					$lineval = $record;
				}
				else
				{
					if ($lineval lt $record)
					{
						print "$line:";
						print OUTPUT "$line ";
					}
					else
					{
						print "$line|";
						print OUTPUT "$line \n";

					}
					$lineval = $record;
					
				}
				$line=1;
				
			}
			else
			{
				$line++;
				
			}
		}
	print "\n";
	close(CHECKBOOK);
	close(OUTPUT);


