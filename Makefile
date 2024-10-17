
program = "~/cfs/redis/src/redis-benchmark -q -n 1000000 -c 100"

run:
	@python test.py $(program)