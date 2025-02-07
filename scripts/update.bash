#!/usr/bin/env bash
echo "Making '$1' by applying '$3' to '$2'."

ofile="$1"
shift

echo "Create $ofile by applying $2 to $1" | python3 ../scripts/llama_file_flat.py "$@" | ../build/complete | {
	while IFS= read -r line
	do
  		if [[ "$line" =~ ^\`\`\` ]]; then
			break
		fi
		echo "$line" >&2
	done
	echo "$ofile" >&2
	while IFS= read -r line
	do
		if [ "$line" = "\`\`\`" ]; then
			break;
		fi
		echo "$line"
	done
	cat >&2
} > "$ofile" && g++ "$ofile" -o "$ofile".out && ./"$ofile".out 2>&1 | tee "$ofile"_output.txt

