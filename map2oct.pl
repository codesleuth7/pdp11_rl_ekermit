#!/usr/bin/perl

my($l);

while( ($l=<>) ) {
   $l =~ s/[\n\r]+//;
   if( $l =~ /^\s*(\S*)\s+0x000000+(\S+)\s+(\S+)/ ) {
      my($pre)=$1; my($x)=$2; my($p)=$3;
      my($n) = hex($x);
      my($o) = sprintf("%06o",$n);
      #print 'pre="'.$pre.'"  hex="'.$x.'" oct="'.$o.'" prg="'.$p.'"'."\n";
      print $l." (".$o.")\n";
   } else {
      print $l."\n";
   }
}
