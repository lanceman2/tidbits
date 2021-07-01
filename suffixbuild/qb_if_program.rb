
# We run the rest of the ruby code in this file unless
# $qb_skip_run_program
#
# The pro of this method is that we keep all the code in one file.
#
# The con of this is that the file is larger and rudy interrupter does a
# pass over code it never runs.
#
#################### begin  long conditional block #######################
unless defined? $qb_skip_run_program and $qb_skip_run_program

