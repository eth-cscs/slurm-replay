#!/bin/bash

ProgName=`basename $0`

#
#  Calculate how many minutes are in a given month
#
MinutesInMonth()
{
#	dim=`cal ${Month} ${Year} | egrep "28|29|30|31" |tail -1 |awk '{print $NF}'`
#	case "$MONTH" in
#		January) MON=01 ;;
#		February) MON=02 ;;
#		March) MON=03 ;;
#		April) MON=04 ;;
#		May) MON=05 ;;
#		June) MON=06 ;;
#		July) MON=07 ;;
#		August) MON=08 ;;
#		September) MON=09 ;;
#		October) MON=10 ;;
#		November) MON=11 ;;
#		December) MON=12 ;;
#	esac

	mm=`echo $Month | sed 's/^0//'`

#	mm=`printf "%d" $Month`

	dim=`cal $mm $Year | tail -2 | head -1 | awk '{print $NF}'`

	expr $dim \* 24 \* 60
}

#
#  Convert Minutes to HH:MM
#
MintoHHMM() {
	Min=$1
	hr=`expr $Min / 60`
	xx=`expr $hr \* 60`
	mn=`expr $Min - $xx`

	printf "%2.2d:%2.2d" $hr $mn
}

#
#  Analyse the month
#
MonthlyReport() {
	sysname=$1
	sn=`echo $sysname | sed 's/%20/ /'`
	downtime=0
	events=0
	failure=0
	graceful=0
	maintenance=0

	for ticket in `$CURL "https://webrt.cscs.ch/Search/Results.html?Format='%3Cb%3E%3Ca%20href%3D%22__WebPath__%2FTicket%2FDisplay.html%3Fid%3D__id__%22%3E__id__%3C%2Fa%3E%3C%2Fb%3E%2FTITLE%3A%23'%2C%0A'%3Cb%3E%3Ca%20href%3D%22__WebPath__%2FTicket%2FDisplay.html%3Fid%3D__id__%22%3E__Subject__%3C%2Fa%3E%3C%2Fb%3E%2FTITLE%3ASubject'%2C%0AStatus%2C%0AQueueName%2C%0AOwner%2C%0APriority%2C%0A'__NEWLINE__'%2C%0A'__NBSP__'%2C%0A'%3Csmall%3E__Requestors__%3C%2Fsmall%3E'%2C%0A'%3Csmall%3E__CreatedRelative__%3C%2Fsmall%3E'%2C%0A'%3Csmall%3E__ToldRelative__%3C%2Fsmall%3E'%2C%0A'%3Csmall%3E__LastUpdatedRelative__%3C%2Fsmall%3E'%2C%0A'%3Csmall%3E__TimeLeft__%3C%2Fsmall%3E'&Order=ASC%7CASC%7CASC%7CASC&OrderBy=id%7C%7C%7C&Query=CF.%7BSystem%7D%20%3D%20'$sysname'%20AND%20'CF.%7BEnd%20date%7D'%20%3C%20'${Year}-${Month}-31%2023%3A59%3A00'%20AND%20'CF.%7BStart%20date%7D'%20%3E%20'${Year}-${Month}-01%2000%3A00%3A00'%20AND%20('CF.%7BTicket%20type%7D'%20LIKE%20'Maintenance'%20OR%20'CF.%7BTicket%20type%7D'%20LIKE%20'System%20Wide%20Outage')&RowsPerPage=50&SavedChartSearchId=new&SavedSearchId=new" 2>/dev/null | grep "/Ticket/" | grep align | cut -f 4 -d \> | sed s/\<.*// `
	do
		if [ $ReportHeader -eq 0 ] ; then
			echo ""                                                                                                                                                                                            >  "$OutageReport"
			echo "$MONTH $Year Monthly Outage Report"                                                                                                                                                          >> "$OutageReport"
			echo "-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------" >> "$OutageReport"
			echo "Ticket  System        Ticket Type         Graceful  Reason      Scheduled Start           Actual Start              Scheduled End             Actual End                Duration (hh:mm,h)"  >> "$OutageReport"
			echo "------  ------------  ------------------  --------  ----------  ------------------------  ------------------------  ------------------------  ------------------------  -------------------" >> "$OutageReport"
			ReportHeader=1
		fi

		$CURL https://webrt.cscs.ch/Ticket/Display.html?id=$ticket 2>/dev/null > $TmpFile

		StartDate=`awk 'BEGIN{ RS="tr" } /Start date:/ {print $0}' < $TmpFile | egrep -v ">|<"`
		SchedStartDate=`awk 'BEGIN{ RS="tr" } /Scheduled start date:/ {print $0}' < $TmpFile | egrep -v ">|<"`
		EndDate=`awk 'BEGIN{ RS="tr" } /End date:/ {print $0}' < $TmpFile | egrep -v ">|<"`
		SchedEndDate=`awk 'BEGIN{ RS="tr" } /Scheduled end date:/ {print $0}' < $TmpFile | egrep -v ">|<"`
		TicketType=`awk 'BEGIN{ RS="tr" } /Ticket type:/ {print $0}' < $TmpFile |  egrep -v "^ " | sed 's!</td>!!' | sed 's/  .*$//'`
		Reason=`awk 'BEGIN{ RS="tr" } /Unscheduled Outage Reason:/ {print $0}' < $TmpFile | egrep -v "^ " | awk '{print $1}'`
		Graceful=`awk 'BEGIN{ RS="tr" } /Graceful Shutdown:/ {print $0}' < $TmpFile | egrep -v "^ " | awk '{print $1}'`
		if [ "$Reason" = "<i>(no" ] ; then
			Reason=""
		fi

		StartStamp=`date -d "$StartDate" +%s`
		EndStamp=`date   -d "$EndDate"   +%s`

		Duration=`expr $EndStamp - $StartStamp`
		Duration=`expr $Duration / 60`
		hhmm=`MintoHHMM $Duration`
		hh=`echo "$Duration / 60" | bc -l`

		if [ "$TicketType" = "Maintenance" ] ; then
			maintenance=`expr $maintenance + $Duration`

		elif [ "$Graceful" = "yes" ] ; then
			graceful=`expr $graceful + $Duration`

		else
			failure=`expr $failure + $Duration`
		fi

		downtime=`expr $downtime + $Duration`


		if [ -e $TmpFile ] ; then
			rm $TmpFile
		fi

		events=`expr $events + 1`

                printf "%6d  %-12s  %-18s  %-8s  %-10s  %24s  %24s  %24s  %24s  %5d (%s,%5.2f)\n"  $ticket "$sn" "$TicketType" "$Graceful" "$Reason"  "$SchedStartDate" "$StartDate" "$SchedEndDate" "$EndDate" $Duration $hhmm $hh >> "$OutageReport"

	done

	#
	#  hours
	#
	totnodes=6367

	maxmin=`expr $totnodes \* $mim`
	maxhours=`expr $maxmin / 60`

	avlmin=`expr $maxmin \- $downtime \* $totnodes`
	avlhours=`expr $avlmin / 60`

	avail=`echo "(1 - ($downtime / $mim)) * 100" | bc -l`

	schedavail=`echo "(1 - (($downtime-$maintenance) / $mim)) * 100" | bc -l`

	maintenance=`echo "$maintenance / 60" | bc -l`
	graceful=`echo "$graceful / 60" | bc -l`
	failure=`echo "$failure / 60" | bc -l`

	printf "%7s  %5.1f  %5.1f  %5.1f  %5d  %5.1f  %5.1f\n" $LastMonthYear $maintenance $graceful $failure $events $avail $schedavail >> "$DataFile"

}

#######################################################
#  MAIN                                               #
#######################################################

month=$1
Year=$2
MONTH=$3

if [ "$MONTHLYRPTS_RPTS" = "" ] ; then
	echo "Please load the monthlyrpts module file first."
	exit
fi

if [ "$MONTHLYRPTS_PLOTS" = "" ] ; then
	echo "Please load the monthlyrpts module file first."
	exit
fi

LastMonthYear=`printf "%2.2d/%4.4d" $month $Year`
Month=`printf "%2.2d" $month`

#
#  Define the various files and locations
#
PlotFileDir=$MONTHLYRPTS_PLOTS
MonthlyRptDir=$MONTHLYRPTS_RPTS/${Year}/${Month}-${MONTH}
OutageReport="$MonthlyRptDir/OutageReport.txt"
DataFile="$MONTHLYRPTS_RPTS/Availability.dat"
OutageFile="$MONTHLYRPTS_RPTS/Outages.dat"

grep "^$LastMonthYear" $DataFile > /dev/null
if [ $? -ne 1 ] ; then
	echo already run for $LastMonthYear, commenting it out.
	sed -i "s!^${LastMonthYear}!\#${LastMonthYear}!" $DataFile
#	echo $?
fi

LastMonthEntry=`tail -1 "$DataFile" | awk '{print $1}'`
ReportHeader=0

TmpFile=/tmp/monthly-tmpdata

mim=`MinutesInMonth`

#
# Username and Password for webrt
#
User=`cat $MONTHLYRPTS_ROOT/etc/.webrt | grep -v "^#" | grep username | awk '{print $2}'`
Pass=`cat $MONTHLYRPTS_ROOT/etc/.webrt | grep -v "^#" | grep password | awk '{print $2}'`

CURL="curl --user $User:$Pass"

#
#  Produce the Monthly Report Data
#
MonthlyReport Daint

echo "" >> "$OutageReport"

sz=`ls -l "$OutageReport" | awk '{print $5}'`
if [ $sz -eq 1 ] ; then
	echo "There were no outages during this reporting period." >> "$OutageReport"
fi

