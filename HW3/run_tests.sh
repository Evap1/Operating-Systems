#!/bin/bash
gcc -o test_queue tests.c queue.c -Wall
./test_queue > tests_output/output_queue.txt

# Remove arrival timestamps (assuming format: "Arrival: X.XXXXXX")
sed -E 's/Arrival: [0-9]+\.[0-9]+//g' tests_output/output_queue.txt > tests_output/output_queue_clean.txt
sed -E 's/Arrival: [0-9]+\.[0-9]+//g' tests_output/expected_queue.txt > tests_output/expected_queue_clean.txt

# Compare cleaned files
diff -u tests_output/expected_queue_clean.txt tests_output/output_queue_clean.txt || echo "Test output differs (excluding arrival times)!"