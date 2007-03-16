#!/bin/sh
############################################################
# This script takes 1 or 2 arguments, a BirthsDeaths logfile is taken as input.  The 2nd argument specifies the output filename.
############################################################

DOT="/usr/local/graphviz-2.12/bin/dot"
DOT_FILLCOLORS='coral deeppink firebrick hotpink indianred lightpink lightsalmon maroon orangered palevioletred pink red salmon tomato violetred'
PLOT_FORMAT="pdf"	# pdf, png, etc.
DOTFILE_PARAMS="
	nodesep=0.3;
//	edge [color=Blue, style=dashed,arrowhead=normal];
	concentrate=true;			// handy!  Collapse 2-edges into a single edge.
	ranksep=equally;			// ranks should be equally distant from one another
	center=true;				// center the graph on the page
	node [color=Red,fontname=Helvetica,shape=invtrapezium];		//style=rounded was kinda nice, but it doesn't look like the critters.
	edge [color=Blue, style=dashed,arrowhead=normal,sametail];

	1[ color=\"0.1,0.15,0.1\", style=filled]
	50[ color=\"0.2,0.3,0.2\", style=filled]
	62[ color=\"0.3,0.45,0.3\", style=filled]
	74[ color=\"0.4,0.6,0.4\", style=filled]
	45[ color=\"0.5,0.75,0.5\", style=filled]
	93[ color=\"0.6,0.9,0.6\", style=filled];
/*
	94[ color=\"0.35,0.25,0.35\", style=filled];
	88[ color=\"0.6,0.5,0.6\", style=filled];
	44[ color=\"0.85,0.75,0.85\", style=filled];
	92[ color=\"0.9,1.0,0.9\", style=filled];
*/

//	a[shape=polygon,sides=5,peripheries=3,color=lightblue,style=filled];
	rankdir=TB;			// Print from top to bottom (the default)
	"


############################################################
BDfile=$(echo "${1}" | sed -e 's/\/$//')       # removing any trailing slash
PLOTfile="$2"

if [ ! -x "$DOT" ]
then
	echo "Could not execute file '$DOT'.  It's either unexecutable or nonexistant.  Fix it."
	exit;
fi

if [ ! -f "$BDfile" ]
then
	echo "This script takes 1 or 2 arguments.
	1) The filename of the polyworld BirthsDeaths.log file
	2) The output filename of the pedigree plot.  If no 2nd argument is specified, the default takes the input filename and appends '.${PLOT_FORMAT}'"
        exit;
fi

if [ -d "$BDfile" ]	# user probably specified a run/ directory instead of the BirthsDeaths.log.  That's okay though, it'll be accounted for.
then
	if [ -f "$BDfile/BirthsDeaths.log" ]
	then
		# update the BDfile to include the BirthsDeaths.log
		BDfile="${BDfile}/BirthsDeaths.log"
	else
		# nope, user didn't specify a run/ directory.  Exit.
		echo "* Do not recognize '$BDfile' as a BirthDeaths.log or a run/ directory.  Exiting."
		exit;
	fi
	
fi



# If no second argument is specified
if [ -z "$PLOTfile" ]
then
	PLOTfile="${BDfile}.${PLOT_FORMAT}"
fi

#firstline looks at the first hundred lines of the BirthsDeaths.log.  It removes all lines beginning with '#' and '%' and then gets the first line (the first line of data)
firstline=$(head -n 100 "$BDfile" | grep -v '^[%#]' | head -n 1)
FL_firstchild=$(echo "$firstline" | cut -d' ' -f3)
numseedcritters=$(echo "$FL_firstchild - 1" | bc)

echo "- Num Seed Critters = $numseedcritters"

#past -> 1978 -> 1980 -> 1982 -> 1983 -> 1985 -> 1986 -> 
# 1987 -> 1988 -> 1989 -> 1990 -> "future"; 



# Only look at lines that are BIRTH events, and then only get columns 3, 4, and 5.
{
echo "digraph hierarchy {"
echo "${DOTFILE_PARAMS}"
grep ' BIRTH ' ${BDfile} | cut -d' ' -f3,4,5 | awk -v firstchild=${FL_firstchild} -F' ' '
BEGIN { 
	DEPTHS[0]=" \"SEED\"";
	CRITTER[0]="";
	HAD_CHILD[0]="";
	MAXDEPTH=0;
	for( i=1; i<firstchild; i++ ) { DEPTHS[0] = DEPTHS[0] " " i; CRITTER[i]=0; }
	}

# Main program
{
	critternum=$1;

	# critterdepth is the average of the depth of the parents, rounded, plus one.
	critterdepth= int( ((CRITTER[$2] + CRITTER[$3]) / 2) + 0.5) + 1;

	# update MAXDEPTH to the new max, make the first node (for the timeline)
	if( critterdepth > MAXDEPTH ) {
		MAXDEPTH=critterdepth;
		DEPTHS[MAXDEPTH]= " \"g" MAXDEPTH "\"";
	}

	DEPTHS[critterdepth]=DEPTHS[critterdepth] " " critternum;
	CRITTER[critternum]=critterdepth

#	print "critterdepth= " critterdepth
	print "{ " $2 " " $3 " }->" critternum

}

END {
	# print the connections of the timeline
	printf("\"SEED\"->")
	# print the connections of the timeline
	for( i=1; i<MAXDEPTH; i++ ) {
		printf( "\"g" i "\"->")
	}
	print " \"g" MAXDEPTH "\";";

	# Now print our depth information
	print "{rank=source;" DEPTHS[0] " }";	# print the SEEDs, they have a special rank.
	for( i=1; i<MAXDEPTH; i++ ) {
#		print "DEPTHS[" i "]: " DEPTHS[i];
		print "{rank=same;" DEPTHS[i] " }"
		}
	# The final children also have a special rank.
	print "{rank=max;" DEPTHS[MAXDEPTH] " }";	# print the SEEDs, they have a special rank.  These could also be "rank=sink", but rank=max is more robustly aesthetic.
}'
echo "}"
} > ',temp'
echo "- Made temp .dot file ',temp'"
echo "- Generating '${PLOTfile}'..."
${DOT} ',temp' -T${PLOT_FORMAT} -o ${PLOTfile}
echo "- Made ${PLOTfile}.  Bringing up the plot.."
open ${PLOTfile}
# rm ,temp
# echo "- Deleted ',temp'"
echo "Done!"
