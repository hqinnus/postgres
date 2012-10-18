#! /usr/bin/perl

use warnings;
use strict;
use warnings::register;

open(LOG, "pplogfile") or die $!;
open(TREES, ">jointrees") or die $!;
open(TEST, ">testlines") or die $!;

my @level1;
my @level2;
my @level3;
my @level4;
my $totalcost = "";
my $readpath = 0;
my $enter_level2 = 0;
my $enter_level3 = 0;
my $enter_level4 = 0;
my $enter_level5 = 0;
my $line = 0;
my $smallest = -1;
my $i = 0;


while(<LOG>)
{
	my $line = $_;
	my $level = readLevel($line);

	print TEST "$level $line";

	#Coming to a new path, print the previous collected path. 
	if($level == 1 && $line =~ /NESTPATH|HASHPATH|MERGEPATH/)
	{
		my $l1;
		my $l2;
		my $l3;
		my $l4;
		my $looks;

		#Collect information on the smallest cost path
		$i++;
		if($smallest eq -1 || $totalcost lt $smallest)
		{
			$line = $i;
			$smallest = $totalcost;
		}

		#Print out nicely according to path information.
		#Very naive approach and not simple, scalable.
		print TREES "(";
		loop1: while($l1 = shift(@level1))
		{
			if($l1 =~ /l2/)
			{
				print TREES "(";
				my $iterate2 = 0;
				loop2: while($l2 = shift(@level2))
				{
					$iterate2++;
					my $iterate3 = 0;
					if($l2 =~ /l3/)
					{
						$iterate3++;
						next if scalar(@level3) eq 0; #if it is a 2-rel query
						print TREES "(";
						loop3: while($l3 = shift(@level3))
						{
							if($l3 =~ /l4/)
							{
								print TREES "(";
								print TREES shift(@level4);
								print TREES " ";
								print TREES shift(@level4);
								print TREES ")";
							}
							else
							{
								print TREES " $l3 ";
							}
							if ($iterate3 == 2)
							{
								print TREES ")";
								next loop2;
							}
						}
						print TREES ")";
					}
					else
					{
						print TREES " $l2 ";
					}
					if ($iterate2 == 2)
					{
						print TREES ")";
						next loop1;
					}
				}
				print TREES ")";
			}
			else
			{
				print TREES " $l1 ";
			}
		}
		print TREES ")";
		print TREES " $totalcost ";

		print TREES "\n";
		init();
		next;
	}

	if($level == 1 && $line =~ /total_cost (.*)/)
	{
		$totalcost = $1;
	}
	elsif($line =~ /{PATH|{INDEXPATH/)
	{
		$readpath = 1;
	}
	elsif($line =~ /NESTPATH|HASHPATH|MERGEPATH/)
	{
		if($enter_level2)
		{
			push(@level1, "l2");
			$enter_level2 = 0;
		}
		elsif($enter_level3)
		{
			push(@level2, "l3");
			$enter_level3 = 0;
		}
		elsif($enter_level4)
		{
			push(@level3, "l4");
			$enter_level4 = 0;
		}
		elsif($enter_level5)
		{
			push(@level4, "l5");
			$enter_level5 = 0;
		}
	}
	elsif($level == 5 && $readpath && $line =~ /parent_relids .* ([0-9])/)
	{
		push(@level4, $1);
		$readpath = 0;
	}
	elsif($level == 4 && $line =~ /outerjoinpath|innerjoinpath/)
	{
		$enter_level5 = 1;
	}
	elsif($level == 4 && $readpath && $line =~ /parent_relids .* ([0-9])/)
	{
		push(@level3, $1);
		$readpath = 0;
	}
	elsif($level == 3 && $line =~ /outerjoinpath|innerjoinpath/)
	{
		$enter_level4 = 1;
	}
	elsif($level == 3 && $readpath && $line =~ /parent_relids .* ([0-9])/)
	{
		push(@level2, $1);
		$readpath = 0;
	}
	elsif($level == 2 && $line =~ /outerjoinpath|innerjoinpath/)
	{
		$enter_level3 = 1;
	}
	elsif($level == 2 && $readpath && $line =~ /parent_relids .* ([0-9])/)
	{
		push(@level1, $1);
		$readpath = 0;
	}
	elsif($level = 1 && $line =~ /outerjoinpath|innerjoinpath/)
	{
		$enter_level2 = 1;
	}
}

print TREES "$line ";
print TREES "$totalcost \n";

close(LOG);
close(TREES);
close(TEST);

sub init{
	@level1 = ();
	@level2 = ();
	@level3 = ();
	@level4 = ();
	$totalcost = 0;
	$readpath = 0;
	$enter_level2 = 0;
	$enter_level3 = 0;
	$enter_level4 = 0;
	$enter_level5 = 0;
}

sub readLevel{
	my $line = $_;

	if($line =~ /^       [\S]/){
		return 1;
	}elsif($line =~ /^      [\S]/){
		return 1;
	}elsif($line =~ /^          [\S]/ || $line =~ /^         [\S]/){
		return 2;
	}elsif($line =~ /^            [\S]/ || $line =~ /^             [\S]/){
		return 3;
	}elsif($line =~ /^               [\S]/ || $line =~ /^                [\S]/){
		return 4;
	}elsif($line =~ /^                   [\S]/ || $line =~ /^                  [\S]/){
		return 5;
	}
}
