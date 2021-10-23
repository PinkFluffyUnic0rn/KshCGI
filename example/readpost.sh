#!/bin/ksh

IFS=
echo "text: ${formdata[text1]}";
echo "password: ${formdata[pass]}";
echo "radio: ${formdata[radio1]}";
echo "checkbox 1: ${formdata[check1]}";
echo "checkbox 2: ${formdata[check2]}";
echo "textarea: ${formdata[textarea1]}"
echo "select: ${formdata[select1]}"
#printf "<!DOCTYPE html>";
#printf "<pre>";

#printenv;

#IFS= read -r line;
#echo $line;
#
#while IFS= read -r line
#do
#	printf "%s" "$line<br>" | sed 's/\t/\&#9/g';
#	echo $line;
#done;

#printf "</pre>";
