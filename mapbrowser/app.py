from dataclasses import dataclass
from dataclasses_json import dataclass_json
from flask import Flask, jsonify, render_template
from flask_sqlalchemy import SQLAlchemy
from networkx import networkx
import json
import itertools

app = Flask(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = 'postgresql://localhost/batmud'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)

@dataclass_json
@dataclass
class Room(db.Model):
    id: str
    shortdesc: str
    longdesc: str
    area: str
    indoors: bool
    exits: str
    id = db.Column(db.Text, primary_key=True)
    shortdesc = db.Column(db.Text)
    longdesc = db.Column(db.Text)
    area = db.Column(db.Text)
    indoors = db.Column(db.Boolean)
    exits = db.Column(db.Text)

@dataclass_json
@dataclass
class Exit(db.Model):
    direction: str
    source: str
    destination: str
    direction = db.Column(db.Text, primary_key=True)
    source = db.Column(db.Text, db.ForeignKey('Room.id'), primary_key=True)
    destination = db.Column(db.Text, db.ForeignKey('Room.id'), primary_key=True)

@app.route('/')
def index():
    areas = db.session.query(Room.area).distinct().all()
    return render_template('index.html', areas=areas)

@app.route('/map/<area>')
def map_route(area):
    return render_template('map.html', area=area)

@app.route('/data/<area>')
def data(area):
    G = networkx.MultiDiGraph()
    rooms = Room.query.filter_by(area=area).all()

    # as nodes, we use the room identifiers, but add json-serializable data
    # formatted for sigma.js as node attrs
    for room in rooms:
        G.add_node(room.id, **room.to_dict())

    exits = Exit.query.filter(Exit.source.in_(list(map(lambda r: r.id, rooms)))).all()
    for exit in exits:
        G.add_edge(exit.source, exit.destination, key=exit.direction)

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
        G.nodes[node]['sg'] = sg
        for src, tgt, direction in G.out_edges(node, keys=True):
            assert src == node
            if not direction in posmod:
                continue
            # TODO: if overlap would happen in subgraph, split to new subgraph
            sg.add_edge(src, tgt, key=direction)
            # if the neighbor is already placed, don't recurse into it
            if 'relx' in sg.nodes[tgt]:
                continue
            dx, dy = posmod[direction]
            sg.nodes[tgt]['relx'] = sg.nodes[src]['relx'] + dx
            sg.nodes[tgt]['rely'] = sg.nodes[src]['rely'] + dy
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
        # print("found subgraph {}".format(len(AG)), file=sys.stderr)
        sg.nodes[node]['relx'] = 0
        sg.nodes[node]['rely'] = 0
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
        srcsg = G.nodes[src]['sg']
        tgtsg = G.nodes[tgt]['sg']
        srcx, srcy = srcsg.nodes[src]['relx'], srcsg.nodes[src]['rely']
        tgtx, tgty = tgtsg.nodes[tgt]['relx'], tgtsg.nodes[tgt]['rely']
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
            AG.nodes[tgtsg]['offx'] = srcx - tgtx
            AG.nodes[tgtsg]['offy'] = srcy - tgty
        edges.append(edge)

    # assign a color to each subgraph and place their nodes on a flat canvas.
    colors = ['#b87a7a', '#7ab87a', '#b8b87a', '#7a7ab8', '#b87ab8', '#7ab8b8',
              '#262626', '#dbbdbd', '#bddbbd', '#dbdbbd', '#bdbddb', '#bddbdb']
    for n, sg in enumerate(AG):
        AG.nodes[sg]['color'] = colors[n % len(colors)]
        # subgraph offset may not be known if there are no edges connecting the
        # subgraph to others. default to 0.
        if 'offx' not in AG.nodes[sg]:
            AG.nodes[sg]['offx'] = 0
            AG.nodes[sg]['offy'] = 0
            AG.nodes[sg]['offx'] += n / len(AG)
            AG.nodes[sg]['offy'] -= n / len(AG)
            # TODO: if node has exits which have no corresponding edges, visualize it
    for node in G:
        sg = G.nodes[node]['sg']
        G.nodes[node]['x'] = sg.nodes[node]['relx'] + AG.nodes[sg]['offx']
        G.nodes[node]['y'] = sg.nodes[node]['rely'] + AG.nodes[sg]['offy']
        G.nodes[node]['color'] = AG.nodes[sg]['color']
        G.nodes[node]['size'] = 1
        obvious_exits = G.nodes[node]['exits']
        if obvious_exits:
            exits = set(obvious_exits.split(','))
            known_edges = set(x[2] for x in G.out_edges(node, keys=True))
            unknown_edges = exits - known_edges
            if unknown_edges:
                G.nodes[node]['borderColor'] = '#ff0000'
                G.nodes[node]['type'] = 'diamond'
                G.nodes[node]['unknown'] = list(unknown_edges)
                # we no longer need the unserializable MultiDiGraph object here
        del G.nodes[node]['sg']

    return jsonify({'nodes': list(G.nodes.values()), 'edges': edges})
