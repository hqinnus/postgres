#! /usr/bin/perl

use warnings;
use strict;
use warnings::register;

open(LOG, "logfile") or die $!;
open(OUT, ">pplogfile") or die $!;

my $status = 0; # 0 for not caring, 1 for find symbol, 2 for reading status

while(<LOG>){
	my $line = $_;

	if($line =~ /\{(.*)PATH/){
		print OUT $line;
	}

	if($line =~ /startup_cost (.*)/){
		print OUT "$line ";
	}elsif($line =~ /total_cost (.*)/){
		print OUT "$line ";
	}elsif($line =~ /parent_relids (.*)/){
		print OUT "$line ";
	}elsif($line =~ /rows (.*)/){
		print OUT "$line ";
	}elsif($line =~  /outerjoinpath/){
		print OUT "$line";
	}elsif($line =~ /innerjoinpath/){
		print OUT "$line";
	}
}

close(LOG);
close(OUT);


sub readPath{
	my $line = $_;
	my @matcharray = ();
	my $entered = 0;

	print OUT "$line \n" if $line =~ /\{(.*)PATH/;
	
	while(<LOG>){
		$line = $_;

		push(@matcharray, "{") if $line =~ /\{/;
		shift(@matcharray) if $line =~ /\}/;

		$entered = 1 if scalar(@matcharray) != 0;
		last if $entered = 1 && scalar(@matcharray) == 0;
	
		if($line =~ /.*startup_cost (.*)/){
			print OUT "$line \n";
		}elsif($line =~ /.*total_cost (.*)/){
			print OUT "$line \n";
		}elsif($line =~ /.*parent_relids (.*)/){
			print OUT "$line \n";
		}elsif($line =~ /.*rows (.*)/){
			print OUT "$line \n";
		}elsif($line =~  /.*outerjoinpath|innerjoinpath/){
			$line = <LOG>;
			warnings::warn("outjoinpath/innerjoinpath structure poor") if $line !~ /\{/;
			print OUT "$line\n";
			readPath($line);
		}
	}
}
