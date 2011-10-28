#!/usr/bin/python
import matplotlib
from matplotlib.colors import Normalize

import numpy as np
from pylab import *
from scipy.stats.stats import sem

params = {'backend': 'pdf',
          'grid.linewidth' : 0.5,
          'lines.linewidth' : 0.5,
          'axes.linewidth' : 0.5,
          'patch.linewidth' : 0.25,
          'xtick.major.pad' : 2,
          'xtick.major.size' : 2,
          'ytick.major.pad' : 2,
          'ytick.major.size' : 2,
          'axes.titlesize': 6,
          'axes.labelsize': 5,
          'text.fontsize': 5,
          'legend.fontsize': 5,
          'xtick.labelsize': 4,
          'ytick.labelsize': 4,
          'text.usetex': False,
          'figure.figsize': (3.4,2.38),
          'figure.subplot.bottom' : 0.125
          }
rcParams.update(params)

from agent import *
import agent.genome as genome
import readcluster as rc

from math import log, sqrt 


def plot(cluster, filename="fingerprints.pdf", funcs=(lambda a: a.id),
         plot_title='Scores by Cluster', cmap='Paired', filter=lambda a: True,
         labels=('id'), plot_pop=False):
    assert len(labels) == len(funcs), "must have a label for every func"
    ac = rc.load(cluster)
    clusters = rc.load_clusters(cluster)
    clusters = [[i, clust] for i,clust in enumerate(clusters) 
                    if len(clust) > 700]
    ids, clusters = zip(*clusters)
    num_clusters = len(clusters)

    ind = np.arange(num_clusters)
    if plot_pop:
        width = 1.0 / (len(funcs) + 2)
    else:
        width = 1.0 / (len(funcs) + 1)
    data = [[np.average([func(a) for a in clust if filter(a)]) 
                for clust in clusters]
                    for func in funcs]
    err = [[np.std([func(a) for a in clust if filter(a)]) /
            np.sqrt(len([func(a) for a in clust if filter(a)]))
               for clust in clusters]
                   for func in funcs]

    if plot_pop:
        data.append([len(clust) / 25346.0 for clust in clusters])
        labels.append('population')

    bars = []
    for offset,datum in enumerate(data):
        b = bar(ind+(offset*width), datum, width,
                color=cm.Paired(float(offset)/len(funcs)),
                yerr=err[offset], capsize=1.5)
        bars.append(b)

    title(plot_title, weight='black')
    ylabel('Normalized Value', weight='bold')
    ylim(ymin=0,ymax=1.0)
    xlabel('Cluster', weight='bold')
    xticks(ind + ( width*len(funcs) / 2), 
           ["%d" % ids[i] 
                for i, clust in enumerate(clusters)])
    legend([b[0] for b in bars], labels, loc='upper left') 

    savefig(filename, dpi=200)


if __name__ == '__main__':
    import sys
    import os.path
    from optparse import OptionParser

    usage = "usage: %prog [options] cluster_file"
    parser = OptionParser(usage)
    options, args = parser.parse_args()

    if args:
        cluster = args[-1]

    if not cluster or not os.path.isfile(cluster):
        print "Must specify a valid cluster file!"
        sys.exit()

    plot(cluster, funcs=(lambda x: Agent(x).genome[4] / 255.0, 
                         lambda x: Agent(x).genome[5] / 255.0,
                         lambda x: Agent(x).genome[7] / 255.0,
                         lambda x: Agent(x).genome[11] / 255.0,
                         lambda x: Agent(x).complexity),
         plot_title="Representative Gene Values and Complexity by Cluster",
         labels=[genome.get_label(4),
                 genome.get_label(5),
                 "MEF",
                 "INGC",
                 'Complexity'
                 ])

