#!/bin/sh

directory=$(echo "${1}" | sed -e 's/\/*$//')
if [ ! -d "$directory" ]
then
	echo "You must specify a polyworld 'run/' directory to make the bestRecent bins with the entire population.";
	exit;
fi

# remove any trailing slashes from the run directory

if [ ! -f "$directory/BirthsDeaths.log" ]
then
	echo "Need a BirthsDeaths.log file, and I don't see any file '$directory/BirthDeaths.log'."; 
	exit;
fi

if [ ! -d "$directory/brain/function" ]
then
	echo "Need a brain/function directory that has the brainFunction files, and I don't see any directory '$directory/brain/function'."; 
	exit;
fi

if [ ! -d "$directory/brain/bestRecent" ]
then
	echo "I need a brain/bestRecent directory to determine what time interval to bin, and I don't see any directory '$directory/brain/bestRecent'.";
	exit;
fi

interval=$(echo "$2" | awk 'int($0) == $0 { print $0 }')	# setting the interval to the forced value (if one isn't specified 'interval' will be an empty string)
if [ -z "$interval" ]
then
	echo "Determing interval dynamically...";
	interval=$(ls -1 "$directory/brain/bestRecent" | awk 'int($1) == $1 && int($1) > 0 { print $0 }' | sort -n | head -1)

	if [ -z "$interval" ]
	then
		echo "I couldn't determine the timestep interval in brain/bestRecent.  Maybe the directory '$directory/brain/bestRecent' is empty?  You can force-specify an interval as \$2"
		exit;
	fi
	if [ "$interval" -le 0 ]
	then
		echo "Got a corrupt value ('$interval') for the interval.  You can force-specify the interval as \$2"
		exit;
	fi
fi
######## Get the Number of Seed Critters (so we'll know to ignore these)
# TODO later.


### If we're missing some files that are only in bestRecent (for some unknown reason), hard link those to brain/function/, in the appropriate format.
NumMoved=0
ls -1 "$directory/brain/bestRecent" | awk 'int($1) == $1 { print $0 }' | sort -n | while read bestRecent
do
#	echo "directory = $bestRecent"
	for filename in `ls -1 $directory/brain/bestRecent/$bestRecent/*_brainFunction_*.txt`
	do
		formatted_filename=$(echo "$filename" | sed -e 's/.*brainFunction/brainFunction/')

		# if the file doesn't exist in brain/function/, hard link to it.
		if [ ! -r "${directory}/brain/function/$formatted_filename" ]
		then
			ln "$filename" "${directory}/brain/function/$formatted_filename"
			echo "Made brain/function/$formatted_filename from $filename"
			NumMoved=$(echo "$NumMoved + 1" | bc)
		fi
	done
done

if [ "$NumMoved" -eq 0 ]
then
	echo "Found no missing files missing from brain/function/"
else
	echo "Linked $NumMoved missing brainFunction files from brain/bestRecent/ to brain/function/"
fi

###### Okay, finished with making sure all of the files are there.

####### Make the brain/bestRecentEntirePopulation
	# If we already have a bestRecentEntirePopulation, delete it.
	if [ -d "$directory/brain/Recent" ]
	then
		rm -rf "$directory/brain/Recent"
	fi
	mkdir "$directory/brain/Recent"
##########
echo "Interval = $interval"

# get all entries for 'DEATH'	TODO: If we ever start recording a 'SMITE' event as different from a 'DEATH' event, we should probably include SMITE too.
data=$(grep "DEATH" "$directory/BirthsDeaths.log" | grep -v '^[#%]')



# For efficiency, we will create all of the destination directories first.
echo "Determining the bins for the Recent/ directory..."
lastevent=$(echo "$data" | tail -n 1 | cut -f1 -d' ')
last_binned_time_of_death=$(echo "" | awk -v time_of_death="$lastevent" -v interval="$interval" '{ if(time_of_death % interval == 0) { print int(time_of_death); } else { print ( (int(time_of_death / interval) + 1) * interval ); } }' )

echo "- LastDeath = $lastevent; LastDeathBinned = $last_binned_time_of_death"
currentstep="${interval}"
echo -n "Creating directories: brain/Recent/" 
while [ "${currentstep}" -le "${last_binned_time_of_death}" ]
do
	mkdir "$directory/brain/Recent/$currentstep"
	if [ -d "$directory/brain/Recent/$currentstep" ]	# did we make the directory?
	then
		echo -n "$currentstep "
	fi
	currentstep=$(echo "$currentstep + $interval" | bc)
done; echo ""	# the 'echo ""' makes a newline.

# Finished making our Recent/ bins.  Now put all of our critters into their Recent/ bins
echo "$data" | while read event
do
	critternum=$(echo "$event" | cut -d' ' -f3)	# get the critternum
	time_of_death=$(echo "$event" | cut -d' ' -f1)	# get the actual timestep of death
	binned_time_of_death=$(echo "" | awk -v time_of_death="$time_of_death" -v interval="$interval" '{ if(time_of_death % interval == 0) { print int(time_of_death); } else { print ( (int(time_of_death / interval) + 1) * interval ); } }' )

#	echo "--- $event ---"
#	echo "critternum = $critternum  ::: timestep of death = $time_of_death"
#	echo "*** intervals = $num_intervals ***"
#	echo " --- $event --- <= $binned_time_of_death";

	printf "\r[timestep: $time_of_death | critter: $critternum] "

	# Finished parsing BirthsDeaths.log, now make sure the record for the critter in brain/function/ exists before we try to link to it (a general sanity check).
	if [ ! -r "$directory/brain/function/brainFunction_${critternum}.txt" ]
	then
		echo "	**Warning: 'brain/function/brainFunction_${critternum}.txt' didn't exist!.**";
		continue;	# skip this entry
	fi

	# Is the brainFunction file invalid?  If so, skip it.
	isInvalid=$(grep -o 'maxBias' ${directory}/brain/function/brainFunction_${critternum}.txt)
	if [ ! -z "${isInvalid}" ]
	then
		echo " ** 'brain/function/brainFunction_${critternum}.txt' is invalid.  Not linking it into brain/Recent **";
		continue;
	fi

	# Now that we know that the file exists, make sure the critter lived for at least a single timestep.
#	numlines=$(cat "$directory/brain/function/brainFunction_${critternum}.txt" | wc -l | tr -d ' ')
#	if [ "$numlines" == 2 ]
#	then
#		echo "	**Warning: 'brain/function/brainFunction_${critternum}.txt' has zero rows.  Skipping it.**";
#		continue;	# skip this entry
#	fi


	# Source file exists, made the destination directory, Lets link it!
	ln -s "$directory/brain/function/brainFunction_${critternum}.txt" "${directory}/brain/Recent/${binned_time_of_death}/brainFunction_${critternum}.txt"

done
printf "\r                                              \rDone!\n"
echo "Making the Seeds..."
./MakeSeedsRecent.sh "${directory}"

echo "Checking for invalid brainFunction files in brain/bestRecent..."
Invalids=$(grep -R -c 'maxBias' ${directory}/brain/bestRecent/ | grep -v 'brainAnatomy' | grep -v ':0$')
if [ ! -z "${Invalids}" ]
then
	echo "** WARNING: Found Invalid brainFunction files in brain/bestRecent! **"
	echo "$Invalids"
else
	echo "No invalids brainFunction files found in brain/bestRecent."
fi
