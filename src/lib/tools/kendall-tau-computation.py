#!/usr/bin/env python3
import optparse
import scipy.stats as stats


def computeKendallTau():
  list1 = []
  list2 = []

  with open(opts.input, 'r') as f:
    for line in f:
      line_split = line.split()
      list1.append(int(line_split[0]))
      list2.append(int(line_split[1]))

  tau, p_value = stats.kendalltau(list1, list2)
  print("%s %f %f" % (opts.delta, (tau + 1.0) / 2.0, p_value))


if __name__ == "__main__":
  # parse options
  parser = optparse.OptionParser()
  parser.add_option("--input")
  parser.add_option("--delta")
  (opts, args) = parser.parse_args()

  computeKendallTau()
