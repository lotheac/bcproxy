Map browser
===========

bcproxy stores room information in a postgresql database. This webapp
is used to display that data.

Setup
=====

- install python modules
  ```
  pip3 install -U Flask SqlAlchemy psycopg2 Flask-SqlAlchemy dataclasses
  ```
- run the web server
  ```
  env FLASK_APP=index.py flask run
  ```
- open your browser at http://127.0.0.1:5000/areas (provided that there
  is room data in your databse)
