#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.

#You should have received a copy of the GNU General Public License
#along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys, os

# Switch to the virtualenv if we're not already there
#INTERP = os.path.expanduser("/srv/www/infrapix-flask/env/bin/python")
#INTERP = os.path.expanduser("/srv/www/venv-infrapix-flask/bin/python")
#if sys.executable != INTERP: os.execl(INTERP, INTERP, *sys.argv)
#sys.path.append(os.getcwd())

INTERP = os.path.join(os.environ['HOME'], 'flask_env', 'bin', 'python')
if sys.executable != INTERP:
    os.execl(INTERP, INTERP, *sys.argv)
sys.path.append(os.getcwd())

from app.app import app as application

#def application(environ, start_response):
#    start_response('200 OK', [('Content-type', 'text/plain')])
#    return ["Hello, world! from %s: %s\n" % (sys.version, sys.executable)]
