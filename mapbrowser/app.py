from dataclasses import dataclass
from datetime import datetime
from flask import Flask, jsonify
from flask_sqlalchemy import SQLAlchemy

app = Flask(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = 'postgresql://localhost/batmud'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)


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

@app.route('/areas')
def areas():
  areas = db.session.query(Room.area).distinct().all()
  return jsonify(areas)
