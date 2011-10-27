'''
Takes a cluster file and plots agent movement and cluster populations across all
time steps for the default run directory.
'''
from multiprocessing import Pool, Manager
import os
import os.path
import subprocess

import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize

from agent import *
from agent.plot import plot_step
import readcluster as rc

def plot(cluster_file, run_dir='../run/', anim_path=None, frame_path=None,
         min_size=None):
    # configure default animation and frame path
    if anim_path is None:
        anim_path = 'anim'
    if frame_path is None:
        frame_path = os.path.join(anim_path, 'frames')

    # ensure paths exist
    if not os.path.isdir(anim_path):
        os.mkdir(anim_path)
    if not os.path.isdir(frame_path):
        os.mkdir(frame_path)
   
    # load clusters
    p = get_population()
    clusters = rc.load_clusters(cluster_file, sort='random')

    # compress, if a threshold is set
    if min_size is not None:
        clusters = compress_clusters(clusters, min_size)
   
    # calculate number of clusters
    n_clusters = len(clusters)
    agent_cluster = rc.agent_cluster(clusters)

    # establish cluster object as a shared memory object.
    #manager = Manager()
    #cluster = manager.dict(agent_cluster)

    start = 1 
    stop = 300
    
    # plot agents at every 3 steps
    for t in xrange(start, stop, 3):
        plot_step(t, p, agent_cluster, n_clusters, frame_path, cmap='Paired')
   
    subprocess.call(("mencoder mf://%s/*.png -o %s/output.avi -mf type=png:w=600:h=800:fps=30 -ovc x264 -x264encopts qp=20" % (frame_path, anim_path) ).split())


def main(run_dir, cluster_file, anim_path=None, frame_path=None):
    plot(cluster_file, run_dir=run_dir,
         anim_path=anim_path, frame_path=frame_path)

def compress_clusters(clusters, min_size=700):
    """
    Takes a set of clusters and collapses all clusters below min_size members
    into a single miscellaneous cluster. Returns the new list of clusters.
    """
    # initialize new cluster list and misc cluster
    new_clusters = []
    misc_cluster = []

    # append cluster to new cluster if over threshold, otherwise extend misc
    for cluster in clusters:
        if len(cluster) >= min_size:
            new_clusters.append(cluster)
        else:
            misc_cluster.extend(cluster)

    # add misc cluster to list of clusters
    new_clusters.append(misc_cluster)
    
    return new_clusters 

if __name__ == '__main__':
    import sys

    assert len(sys.argv) > 1, "Must specify a cluster file!"
    assert os.path.isfile(sys.argv[-1]), "Must specify a valid file!"

    anim_path = 'anim_%s' % sys.argv[-1][:-4]
    frame_path = os.path.join(anim_path, 'frames')
    
    cluster_file = sys.argv[-1]

    main('../run/', cluster_file, anim_path, frame_path)


