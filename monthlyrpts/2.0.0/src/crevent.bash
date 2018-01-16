#!/bin/bash

EnterEvent()
{
	echo -n "Please enter month [1-12]: "
	read mnth

	case $mnth in
		1)  MONTH=January;;
		2)  MONTH=February;;
		3)  MONTH=March;;
		4)  MONTH=April;;
		5)  MONTH=May;;
		6)  MONTH=June;;
		7)  MONTH=July;;
		8)  MONTH=August;;
		9)  MONTH=September;;
		10) MONTH=October;;
		11) MONTH=November;;
		12) MONTH=December;;
		*)  echo "Bad Month $rsp"; exit;;
	esac

	mnth=`printf "%2.2d" $mnth`

	echo -n "Please enter which day of the month [1-31]: "
	read rsp
	DOM=$rsp

	echo -n "Please enter the year of the event: "
	read rsp
	YEAR=$rsp

	echo -n "Please enter a short label for the event: "
	read rsp
	LABEL="$rsp"

	echo "You are about to record the following event:"
	echo ""
	echo "$MONTH $DOM, $YEAR: $LABEL"
	echo ""
	echo -n "continue(y/n): "
	read rsp
	if [ "$rsp" != "y" -a "$rsp" != "Y" ] ; then
		echo aborting...
		exit
	fi

	echo "Recording the event..."

	#BASE=/users/cardo/work/monthlyrpts/reports
	BASE=$MONTHLYRPTS_RPTS
	RPRT=$YEAR/${mnth}-${MONTH}/${MONTH}Events.txt

	if [ ! -d `dirname $BASE/$RPRT` ] ; then
		mkdir -p `dirname $BASE/$RPRT`
	fi

	echo ""                                                    >> $BASE/$RPRT
	echo "#proc   line"                                        >> $BASE/$RPRT
	echo "        linedetails: color=coral"                    >> $BASE/$RPRT
	echo "        points:      $DOM(s) min(s) $DOM(s) max(s)"  >> $BASE/$RPRT
	echo ""                                                    >> $BASE/$RPRT
	echo "#proc   annotate"                                    >> $BASE/$RPRT
	echo "        location:    $DOM(s) min(s)"                 >> $BASE/$RPRT
	echo "        clip:        yes"                            >> $BASE/$RPRT
	echo "        verttext:    yes"                            >> $BASE/$RPRT
	echo "        textdetails: color=coral"                    >> $BASE/$RPRT
	echo "        text:        $LABEL"                         >> $BASE/$RPRT

	chmod 0770 $BASE/$RPRT
}

#
#  MAIN
#

if [ "$MONTHLYRPTS_RPTS" = "" ] ; then
	echo "Please load the monthlyrpts module file first."
	exit
fi

while [ 1 ] ; do
	echo -n "Do you wish to record an event? (y/n) "
	read rsp
	if [ "$rsp" != "y" -a "$rsp" != "Y" ] ; then
		echo exiting...
		exit
	fi
	EnterEvent
done
