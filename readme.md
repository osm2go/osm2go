Projects
--------

OSM2Go uses projects to organize the work. This happens for 
mainly two reasons:

1. The world is too large to be handled by a handheld device, so the
   concept of projects breaks the world down into little pieces a user
   is working on.

2. A handheld device is likely not always online. Thus changes are done
   locally, stored in the project and uploaded/synched at a later time.


A project consists of several parts:

- The project file itself containing the projects name, the geographic
  area it refers to and information required to up- and download data.

- The osm file being the information downloaded from the OpenStreetMap
  servers and containing a snapshot of the state of geographic area
  specified in the project file at the time of download. The osm file
  is never touched by osm2go unless it is overwritten by a newer version.

- A diff file containing the changes the user has made to the data in the
  osm file and which has not yet been uploaded to the osm server.

Uploading
---------

The upload is a delicate step as this actually alters the OpenStreetMap
main database. This also is the only step that alters it, so unless you
perform an update you can play around with the map as you want without 
risking to destroy anything.

So you actually have decided to perform an upload. You've changed some 
ways or nodes. These changes are currently in the project diff file
and you may actually leave and restart osm2go without losing those 
changes. Selecting upload from the menu will first give you a raw overview
of what will happen. You'll be told how many way and nodes you've changed,
how many have been deleted and how many have been created newly. Currently
the "relation" row will always contain zeros as osm2go does not support
relations yet. But please take a short look at the numbers presented for
ways and nodes. Do they make sense? Do they e.g. indicate that something is
to be deleted, but you didn't delete anything? Please don't go ahead then,
but try to figure out what happened.

If everything looks reasonable, then enter your OpenStreetMap account
data into the username and password fields and click ok. A new window will
open containing a text buffer. This is the upload log. It will record 
basic information about your upload and may be especially useful if something
goes wrong. Let's hope you entered your username and the password correctly.
Then the upload should succeed. This usually only takes a few seconds.
If your upload went fine and you actually changed data on the servers 
database, osm2go will then re-download the entire project area from the
server. It will free the map and redraw it on basis of the newly downloaded
data which now hopefully includes the changes you just uploaded. You should
see the same map as before, but this time it comes entirely from data stored
on the main server. Congratulations, you just contributed to the OpenStreetMap
project!

You can go ahead and continue editing and uploading. OSM2Go will take care of
your changes and make sure everything is stored until you upload it to the
server so the server takes over the maintenance of your contributions.

Getting started
---------------

Getting started with osm2go is not dangerous since osm2go works offline 
most of the time and does not touch any data stored in the OpenStreetMap
database unless being asked to do so.
