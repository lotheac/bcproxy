#!/usr/bin/env python3

import networkx
import psycopg2
import sys
import json
import random
import itertools

# TODO: cardinal-position map drawing works very well. Figure out how to split
# into subgraphs that don't need explicit positioning, OR, draw as completely
# separate graphs (losing connecting edges maybe, but get some other method?)

# we ALSO need subgraphs for disconnected nodes (ie. nodes not found in the
# traversal; use them as new starting points for traversal until we have
# visited the entire forest)

G = networkx.MultiDiGraph()

conn = psycopg2.connect('dbname=batmud')
cur = conn.cursor()

cur.execute('select room.id, shortdesc, longdesc, indoors, exits from room '
            'where area=%s', (sys.argv[1],))
# as nodes, we use the room identifiers, but add json-serializable data
# formatted for sigma.js as node attrs
for row in cur:
    node = dict(zip(['id', 'label', 'longdesc', 'indoors', 'exits'], row))
    node['size'] = 1
    G.add_node(node['id'], **node)

cur.execute('select direction, source, destination from exit left join room '
            'on room.id=source where room.area=%s', (sys.argv[1],))
for row in cur:
    direction, src, tgt = row
    G.add_edge(src, tgt, key=direction)

posmod = {
    'north':        (0, -1),
    'south':        (0, 1),
    'west':         (-1, 0),
    'east':         (1, 0),
    'northwest':    (-1, -1),
    'northeast':    (1, -1),
    'southwest':    (-1, 1),
    'southeast':    (1, 1),
}

# BatMUD areas often have multiple "levels".
# Split the graph into subgraphs: for each node not yet visited, recursively
# traverse to neighbors on cardinal directions, positioning each node onto a
# grid.
# AG is a graph of the subgraphs, ie. it describes how levels connect to each
# other.
AG = networkx.DiGraph()
visited = set()

def traverse_cardinal(sg, node):
    G.node[node]['sg'] = sg
    for src, tgt, direction in G.out_edges(node, keys=True):
        assert src == node
        if not direction in posmod:
            continue
        # TODO: if overlap would happen in subgraph, split to new subgraph
        sg.add_edge(src, tgt, key=direction)
        # if the neighbor is already placed, don't recurse into it
        if 'relx' in sg.node[tgt]:
            continue
        dx, dy = posmod[direction]
        sg.node[tgt]['relx'] = sg.node[src]['relx'] + dx
        sg.node[tgt]['rely'] = sg.node[src]['rely'] + dy
        # recurse from here so that we visit all nodes reachable on
        # cardinal directions
        traverse_cardinal(sg, tgt)
    visited.add(node)

for node in G:
    if node in visited:
        continue
    sg = networkx.MultiDiGraph()
    AG.add_node(sg)
    sg.add_node(node)
    print("found subgraph {}".format(len(AG)), file=sys.stderr)
    sg.node[node]['relx'] = 0
    sg.node[node]['rely'] = 0
    traverse_cardinal(sg, node)

# all nodes are split into subgraphs.
# now, deal with the edges.
edges = []
count = itertools.count()
for src, tgt, direction in G.edges:
    edge = {
        # sigma.js wants unique edge ids
        'id': next(count),
        'source': src,
        'target': tgt,
    }
    srcsg = G.node[src]['sg']
    tgtsg = G.node[tgt]['sg']
    srcx, srcy = srcsg.node[src]['relx'], srcsg.node[src]['rely']
    tgtx, tgty = tgtsg.node[tgt]['relx'], tgtsg.node[tgt]['rely']
    if srcsg == tgtsg:
        if direction in posmod:
            if posmod[direction] != (tgtx - srcx, tgty - srcy):
                edge['label'] = direction
                edge['type'] = 'curvedArrow'
    else:
        # edge between subgraphs
        AG.add_edge(srcsg, tgtsg)
        edge['label'] = direction
        edge['type'] = 'curvedArrow'
        edge['color'] = '#99e'
        # offset for global position
        # FIXME: this offset is relative to srcsg, not global position - the
        # end result is therefore a bit off
        AG.node[tgtsg]['offx'] = srcx - tgtx
        AG.node[tgtsg]['offy'] = srcy - tgty
    edges.append(edge)

# assign a color to each subgraph and place their nodes on a flat canvas.
colors = ['#b87a7a', '#7ab87a', '#b8b87a', '#7a7ab8', '#b87ab8', '#7ab8b8',
        '#262626', '#dbbdbd', '#bddbbd', '#dbdbbd', '#bdbddb', '#bddbdb']
for n, sg in enumerate(AG):
    AG.node[sg]['color'] = colors[n % len(colors)]
    # subgraph offset may not be known if there are no edges connecting the
    # subgraph to others. default to 0.
    if 'offx' not in AG.node[sg]:
        AG.node[sg]['offx'] = 0
        AG.node[sg]['offy'] = 0
    AG.node[sg]['offx'] += n / len(AG)
    AG.node[sg]['offy'] -= n / len(AG)
# TODO: if node has exits which have no corresponding edges, visualize it
for node in G:
    sg = G.node[node]['sg']
    G.node[node]['x'] = sg.node[node]['relx'] + AG.node[sg]['offx']
    G.node[node]['y'] = sg.node[node]['rely'] + AG.node[sg]['offy']
    G.node[node]['color'] = AG.node[sg]['color']
    obvious_exits = G.node[node]['exits']
    if obvious_exits:
        exits = set(obvious_exits.split(','))
        known_edges = set(x[2] for x in G.out_edges(node, keys=True))
        unknown_edges = exits - known_edges
        if unknown_edges:
            G.node[node]['borderColor'] = '#ff0000'
            G.node[node]['type'] = 'diamond'
            G.node[node]['unknown'] = list(unknown_edges)
    # we no longer need the unserializable MultiDiGraph object here
    del G.node[node]['sg']

top = {'nodes': list(G.node.values()), 'edges': edges}
print(json.dumps(top, indent=2))
