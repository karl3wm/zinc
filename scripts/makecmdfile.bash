#!/usr/bin/env bash
fn="$(date --iso)-$1"_output.txt
echo ">'$fn'"
{
	echo "\$ $@"
	"$@" 2>&1
} | tee "$fn"
echo ">'$fn'"
