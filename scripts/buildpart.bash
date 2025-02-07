#!/usr/bin/env bash

cmd="$1"
shift
CR="$(tput cr)"
EL="$(tput el)"

echo "$cmd" > commit_template.txt

echo "$cmd" | python3 ../zinc/scripts/llama_file_flat.py "$@" | ../zinc/build/complete | {
	FN=''
	while IFS= read -r line; do
		echo "$line" >&2
		if [[ "$line" =~ ^(\*)(\*)([^\(\)]+\.[^\(\)\ ]+)\*\*$ ]] ||
		   [[ "$line" =~ ^(Also, I\'ve updated|Here\ ?i?\'?s|And here is)\ the\ (modified\ |updated\ |corrected\ |content\ of\ |content\ of\ the\ |)\`([^\`]+)\` ]] ||
		   [[ "$line" =~ ^(Also, I\'ve updated|Here\ ?i?\'?s|And here is)\ the\ (modified\ |updated\ |corrected\ |content\ of\ |content\ of\ the\ |)([^\`\ ]+\.[^\`\ ]+) ]] ||
		   [[ "$line" =~ ^(Here\ ?i?\'?s)\ (a\ modified|an\ updated|a\ corrected)\ version\ of\ the\ \`([^\`]+)\` ]]
		then
			FN="${BASH_REMATCH[3]}"
		else
			if [[ "$line" =~ ^\`\`\` ]] || ( [ -z "$line" ] && [ -n "$FN" ] ); then
				if [ -z "$FN" ]; then
					FN=/dev/stdout
				fi
				{
					if [ -z "$line" ]; then
						IFS= read -r line
						if [[ "$line" =~ ^\`\`\` ]]
						then
							echo "$line" >&2
						elif [[ "$line" =~ ^(\*)(\*)([^\(\)]+\.[^\(\)\ ]+)\*\*$ ]]
						then
							FN="${BASH_REMATCH[3]}"
							continue
						else
							echo "$line"
						fi
					fi
					while IFS= read -r line; do
						if [ "$line" = "\`\`\`" ]; then
							echo "$EL$line" >&2
							break;
						fi
						echo "$line"
						echo -n "$EL$line$CR" >&2
					done
				} > "$FN"
			fi
			FN=''
		fi
	done
}

git add .zinc/logs
