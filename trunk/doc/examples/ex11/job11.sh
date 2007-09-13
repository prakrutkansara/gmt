#!/bin/sh
#		$Id: job11.sh,v 1.10 2007-09-13 17:25:50 remko Exp $
#		GMT EXAMPLE 11
#
# Purpose:	Create a 3-D RGB Cube
# GMT progs:	gmtset, grdimage, grdmath, pstext, psxy
# Unix progs:	rm

# Use psxy to plot "cut-along-the-dotted" lines.

gmtset TICK_LENGTH 0 COLOR_MODEL rgb CHAR_ENCODING Standard+

psxy cut-here.dat -Wthinnest,. -M -R-51/306/0/1071 -JX3.5i/10.5i -X2.5i -Y0.5i \
	-P -U/-2.0i/-0.2i/"Example 11 in Cookbook" -K > example_11.ps

# First, create grids of ascending X and Y and constant 0.
# These are to be used to represent R, G and B values of the darker 3 faces of the cube.

grdmath -I1 -R0/255/0/255 X = x.grd
grdmath -I1 -R Y = y.grd
grdmath -I1 -R 0 = c.grd

grdimage x.grd y.grd c.grd -JX2.5i/-2.5i -R -K -O -X0.5i >> example_11.ps
psxy -M rays.dat -J -R -K -O -Bwesn >> example_11.ps
echo 128 128 12 -45 1 MC "60\217" | pstext -Gwhite -J -R -K -O >> example_11.ps
echo 102  26 12 -90 1 MC 0.4 | pstext -Gwhite -J -R -K -O >> example_11.ps
echo 204  26 12 -90 1 MC 0.8 | pstext -Gwhite -J -R -K -O >> example_11.ps

grdimage x.grd c.grd y.grd -JX2.5i/2.5i -R -K -O -Y2.5i >> example_11.ps
psxy -M rays.dat -J -R -K -O -Bwesn >> example_11.ps
echo 128 128 12 45 1 MC "300\217" | pstext -Gwhite -J -R -K -O >> example_11.ps
echo 26 102 12 0 1 MC 0.4 | pstext -Gwhite -J -R -K -O >> example_11.ps
echo 26 204 12 0 1 MC 0.8 | pstext -Gwhite -J -R -K -O >> example_11.ps

grdimage c.grd x.grd y.grd -JX-2.5i/2.5i -R -K -O -X-2.5i >> example_11.ps
psxy -M rays.dat -J -R -K -O -Bwesn >> example_11.ps
echo 128 128 12 135 1 MC "180\217" | pstext -Gwhite -J -R -K -O >> example_11.ps
echo 102 26 12 90 1 MC 0.4 | pstext -Gwhite -J -R -K -O >> example_11.ps
echo 204 26 12 90 1 MC 0.8 | pstext -Gwhite -J -R -K -O >> example_11.ps

# Second, create grids of descending X and Y and constant 255.
# These are to be used to represent R, G and B values of the lighter 3 faces of the cube.

grdmath -I1 -R 255 X SUB = x.grd
grdmath -I1 -R 255 Y SUB = y.grd
grdmath -I1 -R 255       = c.grd

grdimage x.grd y.grd c.grd -JX-2.5i/-2.5i -R -K -O -X2.5i -Y2.5i >> example_11.ps
psxy -M rays.dat -J -R -K -O -Bwesn >> example_11.ps
echo 128 128 12 225 1 MC "240\217" | pstext -J -R -K -O >> example_11.ps
echo 102 26 12 270 1 MC 0.4 | pstext -J -R -K -O >> example_11.ps
echo 204 26 12 270 1 MC 0.8 | pstext -J -R -K -O >> example_11.ps

grdimage c.grd y.grd x.grd -JX2.5i/-2.5i -R -K -O -X2.5i >> example_11.ps
psxy -M rays.dat -J -R -K -O -Bwesn >> example_11.ps
echo 128 128 12 -45 1 MC "0\217" | pstext -J -R -K -O >> example_11.ps
echo 26 102 12 0 1 MC 0.4 | pstext -J -R -K -O >> example_11.ps
echo 26 204 12 0 1 MC 0.8 | pstext -J -R -K -O >> example_11.ps

grdimage x.grd c.grd y.grd -JX-2.5i/2.5i -R -K -O -X-2.5i -Y2.5i >> example_11.ps
psxy -M rays.dat -J -R -K -O -Bwesn >> example_11.ps
echo 128 128 12 135 1 MC "120\217" | pstext -J -R -K -O >> example_11.ps
echo 26 102 12 180 1 MC 0.4 | pstext -J -R -K -O >> example_11.ps
echo 26 204 12 180 1 MC 0.8 | pstext -J -R -K -O >> example_11.ps
echo 200 200 16 225 1 MC GMT 4 | pstext -J -R -O >> example_11.ps

rm -f *.grd .gmt*
