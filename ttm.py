#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This software is released under the terms of the Apache
# License version 2.  For details of the license, see
# http://www.apache.org/licenses/LICENSE-2.0.

# TODO:
# - replace class Buffer with simple list
##################################################

VERSION = u"1.0"

DEBUG = False

##################################################
# Platform: Windows versus *nix-like

WINDOWS = False

##################################################
# Conditional execution
IMPLEMENTED = False

##################################################

import sys
import getopt
import time
import string
import codecs
import traceback

##################################################
# Constants

EMPTY = u""

EOF = ""  # for file.read()

MININT = -2147483647
MAXINT = 2147483647

# Assign special meaning to some otherwise illegal utf-16 character values

SEGMARK = 0xE000
CREATE = 0xF000
MARKMASK = 0xF000

MAXMARKS = 62
MAXARGS = 63
MAXINCLUDES = 1024
MAXEOPTIONS = 1024
MAXINTCHARS = 32

NUL = (u'\u0000')
EOS = NUL
COMMA = u','
SEMICOLON = u';'
LPAREN = u'('
RPAREN = u')'
LBRACKET = u'['
RBRACKET = u']'
LANGLE = u'<'
RANGLE = u'>'
LBRACE = u'{'
RBRACE = u'}'
SHARP=u'#'
ESCAPE=u'\\'
NL=u'\n'

DFALTBUFFERSIZE = (1 << 10)
DFALTSTACKSIZE = 64
DFALTEXECCOUNT = (1 << 20)

CONTEXTLEN = 20

CREATEFORMAT = "{0:04d}"
SEGFORMAT = "{0:02d}"

#Mnemonics
NESTED = 1
KEEPESCAPE = 1
TOSTRING = 1
NOTTM = None
TRACING = 1
INCR = 1
DECR = -1
ACTIVE = True

if DEBUG:
  PASSIVEMAX = 20
  ACTIVEMAX = 20
SHOWMAX=20

# TTM Flags
FLAG_EXIT = 1
FLAG_TRACE = 2
FLAG_BARE = 4
FLAG_TESTING = 8 # make stderr = stdout

##################################################
# Error Numbers
ENOERR = 0  # No error; for completeness
ENONAME = 1  # Dictionary Name or Character Class Name Not Found
ENOPRIM = 2  # Primitives Not Allowed
EFEWPARMS = 3  # Too Few Parameters Given
EFORMAT = 4  # Incorrect Format
EQUOTIENT = 5  # Quotient Is Too Large
EDECIMAL = 6  # Decimal Integer Required
EMANYDIGITS = 7  # Too Many Digits
EMANYSEGMARKS = 8  # Too Many Segment Marks
EMEMORY = 9  # Dynamic Storage Overflow
EPARMROLL = 10  # Parm Roll Overflow
EINPUTROLL = 11  # Input Roll Overflow
#if IMPLEMENTED :
EDUPLIBNAME = 12  # Name Already On Library
ELIBNAME = 13  # Name Not On Library
ELIBSPACE = 14  # No Space On Library
EINITIALS = 15  # Initials Not Allowed
EATTACH = 16  # Could Not Attach
#endif
EIO = 17  # An I/O Error Occurred
#if IMPLEMENTED :
ETTM = 18  # A TTM Processing Error Occurred
ESTORAGE = 19  # Error In Storage Format
#endif
ENOTNEGATIVE = 20
# Error messages new to this implementation
ESTACKOVERFLOW = 30  # Leave room
ESTACKUNDERFLOW = 31
EBUFFERSIZE = 32  # Buffer overflow
EMANYINCLUDES = 33  # Too many includes (obsolete)#
EINCLUDE = 34  # Cannot read Include file
ERANGE = 35  # index out of legal range
EMANYPARMS = 36  #  # parameters > MAXARGS
EEOS = 37  # Unexpected end of string
EASCII = 38  # ASCII characters only
ECHAR8 = 39  # Illegal 8-bit character set value
EUTF32 = 40  # Illegal utf-32 character set
ETTMCMD = 41  # Illegal  #<ttm> command
ETIME = 42  # gettimeofday failed
EEXECCOUNT = 43  # too many execution calls
# Python additions
EARITHMETIC = 44  # arithmetic op failed (e.g. divide by zero)

# Default case
EOTHER = 99

code = 0

UTF8Writer = codecs.getwriter('utf8')
UTF8Reader = codecs.getreader('utf8')

def ERR(i):
  global code
  code = i

#########################
# Class variables

eoptions = []
argoptions = []

##################################################
# Flag  functions

# functions to set/clear/test these marks
def setMark(w, mark): return w | mark


def clearMark(w, mark): return w & ~ mark


def testMark(w, mark):
  if (w & mark) == 0:
    return 0
  return 1

def issegmark(c):
  return ((ord(c) & MARKMASK) == SEGMARK)

def iscreate(c):
  return ((ord(c) & MARKMASK) == CREATE)

def iscontrol(c): return (ord(c) < ord(' ') or ord(c) == 127)

def iswhitespace(c): return (iscontrol(c) or ord(c) == ord(' '))

def isdec(c): return (ord(c) >= ord('0') and ord(c) <= ord('9'))

def ishex(c): return ((ord(c) >= ord('0') and ord(c) <= ord('9'))
                      or (ord(c) >= ord('a') and ord(c) <= ord('f'))
                      or (ord(c) >= ord('A') and ord(c) <= ord('F')))

##################################################
# Misc. functions

def fromhex(c):
  ic = ord(c)
  if ic >= ord('0') and  ic <= ord('9'):
    return (ic - ord('0'))
  elif ic >= ord('a') and ic <= ord('f'):
    return (ic - ord('a')) + 10
  elif ic >= ord('A') and ic <= ord('F'):
    return (ic - ord('a')) + 10
  else:
    return -1


#########################
# Utility class declarations

# Define the limits to prevent
# run-away computations.

class Limits:
  def __init__(self, buffersize, stacksize, execcount):
    self.buffersize = buffersize
    self.stacksize = stacksize
    self.execcount = execcount

    #end class Limits

#  Define a ttm frame
class Frame:
  def __init__(self, active=True):
    self.args = []
    self.active = active  # true => '#<' false => '##<'
    self.result = None

  def reset(self):
    self.args = []

  def push(self,arg):
    self.args.append(arg)

  def pop(self):
    return self.args.pop()

  def top(self):
    return self.args[len(self.args)-1]

  def argv(self,i):
    if(i < 0 or i >= len(self.args)):
      raise Exception("Framestack: illegal ith index")
    return self.args[i]

  def argc(self): return len(self.args)

  def __str__(self):
    s = SHARP
    if(not self.active): s += SHARP
    s += LANGLE
    if len(self.args) > 0:
       s += self.args[0]
       for i in range(1,len(self.args)):
         s += ";" + self.args[i]
    s += ">"
    if(self.result != None) :
      s += " -> |"
      s += str(self.result)
      s += '|'
    return s

# class Frame


#Name Storage and the Dictionary
# If you add field to this, you need
#  to modify especially ttm_ds
class Name:
  def __init__(self, ttm, name=None):
    self.ttm = ttm
    self.name = name
    self.trace = False
    self.locked = False
    self.builtin = False
    self.sideeffect = False  # side effect only
    self.minargs = 0
    self.maxargs = 0
    self.residual = 0
    self.maxsegmark = 0  # highest segment mark number in use in this string
    self.fcn = None  # builtin == True
    self.body = EMPTY # builtin == False

  def copy(self, other):
    self.name = self.name
    self.trace = other.trace
    self.locked = other.locked
    self.builtin = other.builtin
    self.sideeffect = other.sideeffect
    self.minargs = other.minargs
    self.maxargs = other.maxargs
    self.residual = other.residual
    self.maxsegmark = other.maxsegmark
    self.fcn = other.fcn
    self.body = other.body
  # end copy

  def __str__(self):
    s = "{"
    s += "{0}[{1}:{2}]".format(self.name,self.minargs,self.maxargs)
    if self.trace: s += "T"
    if self.locked: s += "L"
    s += " = "
    if not self.builtin and self.body != None:
      ps1 = printablestring(self.body[0:self.residual],self.ttm.escapec)
      ps2 = printablestring(self.body[self.residual:],self.ttm.escapec)
      ps = "|" + ps1 + "^" + ps2
      if len(ps) >= SHOWMAX : ps = ps[0:SHOWMAX] + "..."
      ps += "|"
      s += ps
    else:
      s += "<builtin>"
    s += "}"
    return s

#end class Name


#Character Classes  and the Charclass table

class Charclass:
  def __init__(self, name, charset, negative=False):
    self.name = name
    self.characters = charset
    self.negative = negative

  def charclassmatch(self, c):
    if self.characters.find(c) < 0:
      if self.negative: return True
      return False
    elif self.negative:
      return False
    return True

  def __str__(self):
    s = LBRACE + str(self.name)
    if(self.negative) : s += u"-"
    s += u"|"
    s += self.characters
    s += u"|"
    s += RBRACE
    return s

#end class Charclass

# Define a buffer of string contents
# for holding e.g. the result of a macro call
# or the current active string or the current
# passive string.
# The buffer has a position and allows insertions
# at position. Note that once space is allocated,
# it is never reduced; however the part of the content
# that has data is recorded. Note also that the string
# is kept as a list of characters since Python has no
# mutable string type.

class StringBuffer:
  def __init__(self, alloc=DFALTBUFFERSIZE):
    self.used = 0  # defines what of
                  # the allocated space is actual content.
    self.pos = 0  # provide a scanner index
    # Pre-allocate the content buffer
    self.content = [NUL] * alloc
  # end __init__

  # Ensure buffer has enough space for n new characters
  def ensure(self, n):
    alloc = len(self.content)
    avail = alloc - self.used
    if (avail >= n): return  # we have enough space
    # We need to extend and materialize the extra space
    need = (n - avail)
    if(need == 0) : need = 1
    self.content += [NUL] * need
  # end ensure

  # Reset buffer
  def reset(self):
    self.used = 0
    self.pos = 0
  #end resetbuffer

  # Open up space at offset to
  # make room for n characters
  def makeroom(self, offset, n):
    self.ensure(n)
    # Make space at location offset
    # Assumes move will not overwrite
    self.content[offset+n:] = self.content[offset:]
  # end makeroom

  # Insert (substring of) text at pos;
  # move everything above pos up n characters
  # Text can be either a string or another
  # string buffer.

  def insert(self, src, offset=-1, n=-1):
    isstring = isinstance(src, basestring)
    start = offset
    if offset < 0: start = 0
    count = n
    if isstring:
      size = len(src)
    else:
      size = src.size()
    if (count < 0):
      count = (size - start)
    # Make space at location pos
    self.makeroom(self.pos,count)
    # insert 
    if isstring:
      x = unicode(src[start:(start + count)])  # may not be necessary
      newdata = list(x)
    else:
      newdata = src.getcontents(start,start + count)
    self.content[self.pos:(self.pos + count)] = newdata
    self.used += count
    return count
  # end insert

  # Overwrite the existing n characters of the buffer
  # starting at offset. Do not overwrite buffer text
  # beginning at endpos.
  # Text can be either a string or another
  # string buffer.

  def overwrite(self, src, offset=-1, n=-1, endpos=-1):
    isstring = isinstance(src, basestring)
    start = offset
    if offset < 0: start = 0
    count = n
    if isstring:
      size = len(src)
    else:
      size = src.size()
    if (count < 0):
      count = (size - start)
    if endpos < 0 or endpos > size :
      endpos = size
    # See if we have room for n characters
    # between start and endpos
    avail = endpos - start
    if avail < n: # no, insert space before endpos
      need = (n - avail)
      # Make space at location endpos
      self.makeroom(endpos,need)
      # fixup 
      if self.pos > endpos:
        self.pos += need
      self.used += need
    # insert 
    if isstring:
      x = unicode(src[start:(start + count)])  # may not be necessary
      newdata = list(x)
    else:
      newdata = src.getcontents(start,endpos)
    self.content[start:endpos] = newdata
    return count
  # end insert

  # append substring of text to the end
  def append(self, src, offset=-1, n=-1):
    savepos = self.pos
    self.pos = self.used
    self.insert(src,offset,n)
  # end append

  # prepend text to the front; modifies pos
  def prepend(self, src, offset=-1, n=-1):
    savepos = self.pos
    self.pos = 0
    count = self.insert(src,offset,n)
    self.pos = savepos + count
  #end prepend

  def size(self):
    return self.used

  # Return result as a (unicode) string
  def getcontents(self, start=0, end=-1):
    stop = end
    if (stop < 0): stop = self.used
    return EMPTY.join(self.content[start:stop])
  # end getcontents

  # Provide single character actions

  def next(self):
    if(self.pos >= self.used) : return EOS;
    c = self.content[self.pos]
    self.pos += 1
    return c
  # end next

  def peek(self, i=0):
    if((self.pos + i)>= self.used) : return EOS;
    c = self.content[self.pos + i]
    return c
  # end peek

  # Append character at end
  def put(self, c):
    if (self.used >= len(self.content)):
      self.ensure(DFALTBUFFERSIZE + len(self.content))
    self.content[self.used] = unicode(c) # Probably not necessary
    self.used += 1
  # end put

  def skip(self, n=1):
    self.pos += n
    if self.pos >= self.used: self.pos = self.used
  # end skip

  def __str__(self):
    if self.used == 0:
      return '""'
    return ('"' + self.getcontents(0, self.pos) + "|"
            + self.getcontents(self.pos,self.used) + '"')
  # end __str__

#end class StringBuffer

##################################################
# TTM Class

# For python, this class only contains the interpreter state
class TTM:
  def __init__(self, buffersize=DFALTBUFFERSIZE, stacksize=DFALTSTACKSIZE, execcount=DFALTEXECCOUNT):
    self.limits = Limits(buffersize, stacksize, execcount)
    self.sharpc = SHARP
    self.openc = LANGLE
    self.closec = RANGLE
    self.semic = SEMICOLON
    self.escapec = ESCAPE
    self.metac = NL

    self.stack = []

    self.stdinclose = False
    self.stdout = sys.stdout
    self.stdoutclose = False
    self.stdin = sys.stdin
    self.stderrclose = False
    self.stderr = sys.stderr

    # Following 2 fields are dictionaries
    self.dictionary = {}
    self.charclasses = {}

    self.flags = 0
    if DEBUG:
      self.flags |= FLAG_TRACE
    #endif

    # record the current processor time
    self.xtimebase = 0.0
    if WINDOWS:
     self.xtimebase = time.clock() # floating point number

    self.exitcode = 0
    self.crcounter = 0

    argv = None  # after options have been removed

    self.active = StringBuffer(DFALTBUFFERSIZE)

  # end __init__

  # This is basic top level scanner.
  def scan(self, ba):
    bp = StringBuffer()
    while True:
      c = ba.peek()  # NOTE that we do not bump here
      if (c == NUL):  # End of active buffer
        break
      elif (c  == self.escapec):
        ba.skip()  # skip the escape
        bp.put(ba.next())
      elif (c == self.sharpc):  # Start of call?
        c1 = ba.peek(1)
        if (c1 == self.openc
            or (c1 == self.sharpc
                and ba.peek(2) == self.openc)):
          # It is a real call
          self.execute(ba, bp)
          if ((self.flags & FLAG_EXIT) != 0):
            return
        else:  # not a call; just pass the '#' along passively
          bp.put(c)
          ba.skip()
      elif (c == self.openc):  # Start of <...> escaping
        # skip the leading lbracket
        depth = 1
        ba.skip()
        while True:
          c = ba.get()
          if (c == NUL): self.fail(EEOS)  # Unexpected EOF
          bp.put(c)
          ba.skip()
          if (c == self.escapec):
            bp.put(ba.next())
          elif (c == self.openc):
            depth += 1
          elif (c == self.closec):
            depth -= 1
            if (depth == 0):
              break  # we are done
          else:  # keep moving
            pass
        # end while
      else:  # non-signficant character
        bp.put(c)
        ba.skip()
    # endwhile
    return bp.getcontents()
  #end scan

  def execute(self, ba, bp): 
    self.limits.execcount -= 1
    if (self.limits.execcount <= 0):
      self.fail( EEXECCOUNT)
    frame = self.pushframe()
    # Skip to the start of the function name
    if (ba.peek(1) == self.openc):
      ba.skip(2)
      frame.active = True
    else:
      ba.skip(3)
      frame.active = False
    # Parse and store relevant pointers into frame.
    self.parsecall(frame, ba)
    if ((self.flags & FLAG_EXIT) != 0):
      self.popframe()
      return
    # Now execute this function, which will leave result in result
    if (frame.argc() == 0): self.fail( ENONAME)
    if (frame.argv(0) == None): self.fail( ENONAME)
    if (len(frame.argv(0)) == 0): self.fail( ENONAME)
    # Locate the function to execute
    if frame.argv(0) in self.dictionary:
      fcn = self.dictionary[frame.argv(0)]
    else:
      self.fail(ENONAME)
    if (fcn.minargs > (frame.argc() - 1)):  # -1 to account for function name
      self.fail(EFEWPARMS)
    if (not fcn.sideeffect):
      frame.result = StringBuffer()
    if ((self.flags & FLAG_TRACE) != 0 or fcn.trace):
      trace(self, True, TRACING)
    if (fcn.builtin):
        fcn.fcn(self, frame, frame.result)
    else:  # invoke the pseudo function "call"
      self.call(frame, fcn.body)

    if ((self.flags & FLAG_EXIT) != 0):
      self.popframe()
      return

    if DEBUG:
      self.stderr.write("result: ")
      if frame.result == None:
        self.stderr.write("None")
      else:
        dbgprint(self,frame.result.getcontents(), '|')
      self.stderr.write('\n')
    #endif

    if ((self.flags & FLAG_TRACE) != 0 or fcn.trace):
      trace(self, False, TRACING)

    # Now, put the result into the active/passive buffer
    if frame.result != None:
      if (frame.active):
        ba.insert(frame.result)
      else :  #frame.passive
        bp.append(frame.result)
    if DEBUG:
      self.stderr.write("context:\n\tpassive=")
      dbgprint(self.stderr,bp.content, '|')
      self.stderr.write("...|\n")
      self.stderr.write("\tactive=|")
      dbgprint(self.stderr,ba.content, '|')
      self.stderr.write("...|\n")
    #endif

    #exiting:
    self.popframe()
  #end exec

  #Construct a frame; leave ba.pos pointing just past the call.
  def parsecall(self, frame, ba):
    tmp = StringBuffer()
    done = False
    while True:
      # collect the the ith arg
      tmp.reset()
      while not done:
        c = ba.peek()  # Note that we do not bump here
        if (c == EOS): self.fail(EEOS)  # Unexpected end of buffer
        if (c == self.escapec):
          ba.skip()
          tmp.next(ba.next())
        elif (c == self.semic or c == self.closec):
          # End of an argument; capture
          if DEBUG:
            self.stderr.write("parsecall: argv[{0}]=".format(frame.argc()))
            dbgprint(self.stderr,tmp, '|')
            self.stderr.write("\n")
          #endif
          ba.skip()  # skip the semi or close
          # store arg in frame
          frame.push(tmp.getcontents())
          if (c == self.closec):
            done = True
            break
          elif (frame.argc() >= MAXARGS):
            self.fail(EMANYPARMS)
          tmp.reset()
        elif (c == self.sharpc):
          # check for call within call
          if (ba.peek(1) == self.openc
              or (ba.peek(1) == self.sharpc
                  and ba.peek(2) == self.openc)):
            # Recurse to compute inner call
            self.execute(ba, tmp)
            if ((self.flags & FLAG_EXIT) != 0): return
        elif (c == self.openc):  # <...> nested brackets
          ba.skip()  # skip leading lbracket
          depth = 1
          while True:
            c = ba.peek()
            if (c == NUL): self.fail( EEOS)  # Unexpected EOF
            elif (c == self.escapec):
              tmp.put(c)
              tmp.put(ba.next())
            elif (c == self.openc):
              tmp.put(c)
              ba.skip()
              depth += 1
            elif (c == self.closec):
              depth -= 1
              ba.skip()
              if (depth == 0): break  # we are done
              tmp.put(c)
            else:
              tmp.put(ba.next())
          # loop for <...>
        else:  # keep moving
          tmp.put(c)
          ba.skip()
      # collect argument for
      if(done) : break  # while !done
    # end while(!done)

  # end parsecall

  ##################################################
  #Execute a non-builtin function

  def call(self, frame, body):
    # Expand the body
    assert (frame.result != None)
    br = frame.result
    if len(body) == 0:
      br.reset()
      return
    crvalue = None # use same cr number for all substitutions
    for c in body:
      if issegmark(c):
        segindex = ord(c) & 0xFF
        if (segindex < frame.argc()):
          arg = frame.argv(segindex)
          br.append(arg)
      elif iscreate(c) :
        # Construct the cr value
        if crvalue == None:
          self.crcounter += 1
          crvalue = self.crcounter
        crval = CREATEFORMAT.format(crvalue)
        size = len(crval)
        br.append(crval, 0, size)
      else:
        br.put(c)
    # end while

  # end call

  ##################################################
  # Built-in Support Procedures

  # Manage the frame stack
  def pushframe(self):
    if (len(self.stack) >= self.limits.stacksize):
      self.fail(ESTACKOVERFLOW)
    frame = Frame(not ACTIVE)
    self.stack.append(frame)
    return frame

  def popframe(self):
    if (len(self.stack) == 0):
      self.fail(ESTACKUNDERFLOW)
    frame = self.stack.pop()
    return frame

  ##################################################
  # Error reporting
  def fail(self, eno):
    self.fatal("({0}) {1}".format(eno, errorstring(eno)))
  #end fail

  def fatal(self, msg):
    self.stderr.write("Fatal error: " + msg + "\n")
    # Dump the frame stack
    dumpstack(self)
    #raise Exception("Fatal exception")
    sys.exit(1)
# end class TTM

##################################################
# Builtin functions
# Note that these are outside the TTM class
##################################################

# Original TTM functions

# Dictionary Operations
def ttm_ap(ttm, frame, br):  # Append to a string
  # Note: br is the result buffer
  arg = frame.argv(1)
  if (arg in ttm.dictionary):
    entry = ttm.dictionary[arg]
  else:  # Define the string
    ttm_ds(ttm,frame, br)
    return
  if (entry.builtin): ttm.fail(ENOPRIM)
  apstring = frame.argv(2)
  aplen = len(apstring)
  body = entry.body
  bodylen = len(body)
  newbody = body + apstring
  entry.body = newbody
  entry.residual = bodylen + aplen
# end ttm_ap

# The semantics of  #<cf>
# have been changed. See ttm.html

def ttm_cf(ttm, frame, br):  # Copy a function
  newname = frame.argv(1)
  oldname = frame.argv(2)
  if oldname in ttm.dictionary:
    oldentry = ttm.dictionary[oldname]
  else:
    ttm.fail(ENONAME)
  if newname in ttm.dictionary:
    newentry = ttm.dictionary[newname]
  else:
    # create a new string object
    newentry = Name(ttm, newname)
    ttm.dictionary[newname] = newentry
  newentry.copy(oldentry)
  pass
#end ttm_cf

def ttm_cr(ttm, frame, br):  # Mark for creation
  arg = frame.argv(1)
  if (arg in ttm.dictionary):
    entry = ttm.dictionary[arg]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)

  body = entry.body
  bodylen = len(body)
  crstring = frame.argv(2)
  crlen = len(crstring)
  if (crlen > 0):  # search only if possible success
    # Search for all occurrences of arg; replace same mark
    offset = entry.residual
    newbody = EMPTY
    while True:
      pos = string.find(body, crstring, offset)
      if (pos < 0):
        newbody += body[offset:]
        entry.body = newbody
        break
      else:
        newbody += body[offset:pos] + unichr(CREATE)
        offset = pos + crlen
# end ttm_cr
# end ttm_cr

def ttm_ds(ttm, frame, br):  # Define string
  arg = frame.argv(1)
  if (arg in ttm.dictionary):
    entry = ttm.dictionary[arg]
  else:
    # create a new string object
    entry = Name(ttm, arg)
    ttm.dictionary[arg] = entry
    # reset as needed
  entry.builtin = False
  entry.minargs = 0
  entry.maxargs = 0
  entry.residual = 0
  entry.maxsegmark = 0
  entry.fcn = None
  entry.body = frame.argv(2)
# end ttm_ds

def ttm_es(ttm, frame, br):  # Erase string
  for i in range(1, frame.argc()):
    strname = frame.argv(i)
    if (strname in ttm.dictionary):
      entry = ttm.dictionary[strname]
      if not entry.locked :
        del ttm.dictionary[strname]
    else:
      pass
#end ttm_es

# Helper function for  #<sc> and  #<ss>
def ttm_ss0(ttm, frame, br):
  arg = frame.argv(1)
  if (arg in ttm.dictionary):
    entry = ttm.dictionary[arg]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)
  body = entry.body
  # pull off the residual prefix
  residual = body[0:entry.residual]
  body = body[entry.residual:]
  bodylen = len(body)
  segcount = 0
  startseg = entry.maxsegmark
  for i in range(2, frame.argc()):
    sstring = frame.argv(i)
    sslen = len(sstring)
    newbody = EMPTY
    # Search for occurrences of arg
    offset = 0
    newbody = EMPTY
    found = False
    while True:
      pos = string.find(body, sstring, offset)
      if (pos < 0):
        newbody += body[offset:]
        body = newbody
        break
      else:
        if (not found):  # first match
          startseg += 1
          found = True
        newbody += body[offset:pos]
        newbody += unichr(int(SEGMARK) | startseg)
        segcount += 1
        offset = pos + sslen
  entry.maxsegmark = startseg
  entry.body = newbody
  return segcount

# ttm_ss0

def ttm_sc(ttm, frame, br):  # Segment and count
  nsegs = ttm_ss0(ttm, frame, br)
  br.append(str(nsegs))

# end ttm_sc

def ttm_ss(ttm, frame, br):  # Segment
  ttm_ss0(ttm, frame, br)

# end ttm_ss

# Name Selection
def ttm_cc(ttm, frame, br):  # Call one character
  arg = frame.argv(1)
  if (arg in ttm.dictionary):
    entry = ttm.dictionary[arg]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)
    # Check for pointing at trailing NUL
  if (entry.residual < len(entry.body)):
    c = entry.body[entry.residual]
    br.append(c)
    entry.residual += 1

# end ttm_cc

def ttm_cn(ttm, frame, br):  # Call n characters
  arg2 = frame.argv(2)
  if arg2 in ttm.dictionary:
    entry = ttm.dictionary[arg2]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)

  # Get number of characters to extract
  try:
    n = int(frame.argv(1))
  except ValueError:
    ttm.fail(EDECIMAL)
  if (n < 0): ttm.fail(ENOTNEGATIVE)

  # See if we have enough space
  bodylen = len(entry.body)
  if (entry.residual >= bodylen):
    avail = 0
  else:
    avail = (bodylen - entry.residual)

  if (n == 0 or avail == 0):
    return

  if (avail < n): n = avail  # return what is available

  # We want n characters starting at residual
  # ok, copy n characters from startn into the return buffer
  br.append(entry.body, entry.residual, n)
  # increment residual
  entry.residual += n

# end ttm_cn

def ttm_cp(ttm, frame, br):  # Call parameter
  arg = frame.argv(1)
  if arg in ttm.dictionary:
    entry = ttm.dictionary[arg]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)

  depth = 0
  p = entry.residual
  while p < len(entry.body):
    c = entry.body[p]
    if (c == ttm.semic):
      if (depth == 0): break  # reached unnested semicolon#
    elif (c == ttm.openc):
      depth += 1
    elif (c == ttm.closec):
      depth -= 1
    p += 1
  #end while
  delta = (p - entry.residual)
  br.append(entry.body, entry.residual, delta)
  entry.residual += delta
  if p < len(entry.body):
    entry.residual += 1
#end ttm_cp

def ttm_cs(ttm, frame, br):  # Call segment
  arg = frame.argv(1)
  if arg in ttm.dictionary:
    entry = ttm.dictionary[arg]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)

  # Locate the next segment mark
  # Unclear if create marks also qualify; assume yes
  p = entry.residual
  while p < len(entry.body):
    c = entry.body[p]
    if issegmark(c) or iscreate(c): break
    p += 1
  delta = (p - entry.residual)
  if (delta > 0):
    br.append(entry.body, entry.residual, delta)
    # set residual pointer correctly
  entry.residual += delta
  if p < len(entry.body): entry.residual += 1
#end ttm_cs

def ttm_isc(ttm, frame, br):  # Initial character scan;
# moves residual pointer
  arg2 = frame.argv(2)
  if arg2 in ttm.dictionary:
    entry = ttm.dictionary[arg2]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)

  arg = frame.argv(1)
  arglen = len(arg)
  t = frame.argv(3)
  f = frame.argv(4)

  # check for initial string match
  if (entry.body.startswith(arg, entry.residual)):
    result = t
    entry.residual += arglen
    slen = len(entry.body)
    if (entry.residual > slen): entry.residual = slen
  else:
    result = f
  br.append(result, 0, len(result))

#end ttm_isc

def ttm_rrp(ttm, frame, br):  # Reset residual pointer
  arg = frame.argv(1)
  if arg in ttm.dictionary:
    entry = ttm.dictionary[arg]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)
  entry.residual = 0

#end ttm_rrp

def ttm_scn(ttm, frame, br):  # Character scan
  arg2 = frame.argv(2)
  if arg2 in ttm.dictionary:
    entry = ttm.dictionary[arg2]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)

  arg = frame.argv(1)
  f = frame.argv(3)

  # check for sub string match
  p = entry.body.find(arg,entry.residual)
  if p < 0:  # no match; return argv[3]
    br.append(f)
  elif p > entry.residual:  # return from residual ptr to location of string
    delta = (p - entry.residual)
    br.append(entry.body, entry.residual, delta)
  else:  # if the match is at the residual ptr, mv ptr
    entry.residual += len(arg)
    bodylen = len(entry.body)
    if (entry.residual > bodylen): entry.residual = bodylen
#end ttm_scn

def ttm_sn(ttm, frame, br):  # Skip n characters
  arg2 = frame.argv(2)
  if arg2 in ttm.dictionary:
    entry = ttm.dictionary[arg2]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)

  try:
    num = int(frame.argv(1))
  except ValueError:
    ttm.fail(EDECIMAL)
  if (num < 0): ttm.fail(ENOTNEGATIVE)

  entry.residual += num
  bodylen = len(entry.body)
  if (entry.residual > bodylen):
    entry.residual = bodylen

#end ttm_sn

def ttm_eos(ttm, frame, br):  # Test for end of string
  arg = frame.argv(1)
  if arg in ttm.dictionary:
    entry = ttm.dictionary[arg]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)
  bodylen = len(entry.body)
  t = frame.argv(2)
  f = frame.argv(3)
  if (entry.residual >= bodylen):
    result = t
  else:
    result = f
  br.append(result, 0, len(result))

#end ttm_eos

# Name Scanning Operations

def ttm_gn(ttm, frame, br):  # Give n characters from argument string#
  snum = frame.argv(1)
  s = frame.argv(2)
  slen = len(s)

  try:
    num = int(snum)
  except ValueError:
    ttm.fail(EDECIMAL)

  if (num > 0):
    if (slen < num): num = slen
    startp = 0
  elif (num < 0):
    num = -num
    startp = num
    num = (slen - num)
  if (num != 0):
    br.append(s, startp, num)

#end ttm_gn

def ttm_zlc(ttm, frame, br):  # Zero-level commas
  s = frame.argv(1) + EOS # avoid having to test against len
  slen = len(s)
  depth = 0
  p = 0
  while True:
    c = s[p]
    if c == EOS: break
    if c == ttm.escapec:
      br.put(c)
      p += 1
      br.put(s[p])
    elif (c == COMMA and depth == 0):
      br.put(ttm.semic);
    elif (c == LPAREN):
      depth += 1
      br.put(c)
    elif (c == RPAREN):
      depth -= 1
      br.put(c)
    else:
      br.put(c)
    p += 1
    # end while
#end ttm_zlc

def ttm_zlcp(ttm, frame, br):  # Zero-level commas and parentheses
# exact algorithm is unknown
  # A(B) and A,B will both give A;B and (A),(B),C will give A;B;C
  s = frame.argv(1) + EOS # Add EOS to avoid having to test p all the time
  slen = len(s)
  p = 0
  depth = 0
  while True:
    c = s[p]
    if c == EOS: break
    if (c == ttm.escapec):
      br.put(c)
      p += 1
      br.put(s[p])
    elif (depth == 0 and c == COMMA):
      if (s[p+1] != LPAREN):
        br.put(ttm.semic)
    elif (c == LPAREN):
        if (depth == 0 and p > 0):
          br.put(ttm.semic)
        if (depth > 0):
          br.put(c)
        depth += 1
    elif (c == RPAREN):
      depth -= 1
      if (depth == 0 and s[p+1] == COMMA):
        pass
      elif (depth == 0 and s[p + 1] == EOS):  # do nothing
        pass
      elif (depth == 0):
        br.put(ttm.semic)
      else:  # depth > 0
        br.put(c)
    else:
      br.put(c)
    p += 1
  # end while
#end ttm_zlcp

def ttm_flip(ttm, frame, br):  # Flip a string
  s = frame.argv(1)
  for p in range(len(s), 0, -1):
    br.put(s[p-1])
#end ttm_flip

def ttm_ccl(ttm, frame, br):  # Call class
  arg = frame.argv(1)
  arg2 = frame.argv(2)

  if arg in ttm.charclasses:
    cl = ttm.charclasses[arg]
  else:
    ttm.fail(ENONAME)
  if arg2 in ttm.dictionary:
    entry = ttm.dictionary[arg2]
  else:
    ttm.fail(ENONAME)

  if (entry.builtin):
    ttm.fail(ENOPRIM)

  # Starting at entry.residual, locate first char not in class
  p = entry.residual
  while p < len(entry.body):
    c = entry.body[p]
    if (cl.negative and cl.characters.find(c) >= 0): break;
    if (not cl.negative and cl.characters.find(c) < 0): break;
    p += 1
  count = (p - entry.residual)
  if (count > 0):
    br.append(entry.body, entry.residual, count)
    entry.residual += count
# end ttm_ccl

# Shared helper for dcl and dncl
def ttm_dcl0(ttm, frame, negative):
  arg = frame.argv(1)
  characters = frame.argv(2)
  if (arg in ttm.charclasses):
    cl = ttm.charclasses[arg]
    cl.negative = negative
    cl.characters = characters
  else:
    # create a new charclass object
    cl = Charclass(arg, characters, negative)
    ttm.charclasses[arg] = cl

# end ttm_dcl0

def ttm_dcl(ttm, frame, br):  # Define a negative class
  ttm_dcl0(ttm, frame, False)

#end ttm_dcl

def ttm_dncl(ttm, frame, br):  # Define a negative class
  ttm_dcl0(ttm, frame, True)

# end ttm_dncl

def ttm_ecl(ttm, frame, br):  # Erase a class
  for i in range(1, frame.argc()):
    clname = frame.argv(i)
    if clname in ttm.charclasses:
      del ttm.charclasses[clname]
# end ttm_ecl

def ttm_scl(ttm, frame, br):  # Skip class
  arg = frame.argv(1)
  arg2 = frame.argv(2)

  if arg in ttm.charclasses:
    cl = ttm.charclasses[arg]
  else:
    ttm.fail(ENONAME)
  if arg2 in ttm.dictionary:
    entry = ttm.dictionary[arg2]
  else:
    ttm.fail(ENONAME)
  if (entry.builtin):
    ttm.fail(ENOPRIM)

  # Starting at entry.residual, locate first char not in class
  p = entry.residual
  while p < len(entry.body):
    c = entry.body[p]
    if (cl.negative and cl.characters.find(c) >= 0): break;
    elif (not cl.negative and cl.characters.find(c) < 0): break;
    p += 1
  count = (p - entry.residual)
  entry.residual += count
# end ttm_scl

def ttm_tcl(ttm, frame, br):  # Test class
  arg = frame.argv(1)
  arg2 = frame.argv(2)
  t = frame.argv(3)
  f = frame.argv(4)

  if arg in ttm.charclasses:
    cl = ttm.charclasses[arg]
  else:
    ttm.fail(ENONAME)
  if arg2 in ttm.dictionary:
    entry = ttm.dictionary[arg2]
    if (entry.builtin):
      ttm.fail(ENOPRIM)
  else:
    entry = None

  if str != None:
    # see if char at entry.residual is in class
    if entry.residual < len(entry.body):
      c = entry.body[entry.residual]
      if (cl.negative and cl.characters.find(c) < 0):
        retval = t
      elif (not cl.negative and cl.characters.find(c) >= 0):
       retval = t
      else:
        retval = f
    else:
      retval = f
  else:
    retval = f
  retlen = len(retval)
  if (retlen > 0):
    br.append(retval, 0, retlen)
#end ttm_tcl

# Arithmetic Operators

def ttm_abs(ttm, frame, br):  # Obtain absolute value
  slhs = frame.argv(1)
  try:
    lhs = int(slhs)
  except ValueError:
    ttm.fail(EDECIMAL)
  if (lhs < 0): lhs = -lhs
  br.append(str(lhs))

# end ttm_abs

def ttm_ad(ttm, frame, br):  # Add
  total = 0
  try:
    for i in range(1, frame.argc()):
      snum = frame.argv(i)
      num = int(snum)
      total += num
    br.append(str(total))
  except ValueError:
    ttm.fail(EDECIMAL)
# end ttm_ad

def ttm_dv(ttm, frame, br):  # Divide and give quotient
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  try:
    lhs = int(slhs)
    rhs = int(srhs)
    try:
      lhs = (lhs / rhs)
    except ZeroDivisionError:
      ttm.fail(EARITHMETIC)
    br.append(str(lhs))
  except ValueError:
    ttm.fail(EDECIMAL)
# end ttm_dv

def ttm_dvr(ttm, frame, br):  # Divide and give remainder
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  try:
    lhs = int(slhs)
    rhs = int(srhs)
    lhs = (lhs % rhs)
    br.append(str(lhs))
  except ValueError:
    ttm.fail(EDECIMAL)
# end ttm_dvr

def ttm_mu(ttm, frame, br):  # Multiply
  total = 1
  try:
    for i in range(1, frame.argc()):
      snum = frame.argv(i)
      num = int(snum)
      total *= num
    br.append(str(total))
  except ValueError:
    ttm.fail(EDECIMAL)

# end ttm_mu

def ttm_su(ttm, frame, br):  # Substract
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  try:
    lhs = int(slhs)
    rhs = int(srhs)
    lhs = (lhs - rhs)
    br.append(str(lhs))
  except ValueError:
    ttm.fail(EDECIMAL)
# end ttm_su

def ttm_eq(ttm, frame, br):  # Compare numeric equal
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  t = frame.argv(3)
  f = frame.argv(4)
  try:
    lhs = int(slhs)
    rhs = int(srhs)
    if (lhs == rhs):
      br.append(t)
    else:
      br.append(f)
  except ValueError:
    ttm.fail(EDECIMAL)
# end ttm_eq

def ttm_gt(ttm, frame, br):  # Compare numeric greater-than
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  t = frame.argv(3)
  f = frame.argv(4)
  try:
    lhs = int(slhs)
    rhs = int(srhs)
    if (lhs > rhs):
      br.append(t)
    else:
      br.append(f)
  except ValueError:
    ttm.fail(EDECIMAL)
# end ttm_gt

def ttm_lt(ttm, frame, br):  # Compare numeric less-than
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  t = frame.argv(3)
  f = frame.argv(4)
  try:
    lhs = int(slhs)
    rhs = int(srhs)
    if (lhs < rhs):
      br.append(t)
    else:
      br.append(f)
  except ValueError:
    ttm.fail(EDECIMAL)
# end ttm_lt

def ttm_eql(ttm, frame, br):  # ? Compare logical equal
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  t = frame.argv(3)
  f = frame.argv(4)
  if cmp(slhs, srhs) == 0:
    br.append(t)
  else:
    br.append(f)
# end ttm_eql

def ttm_gtl(ttm, frame, br):  # ? Compare logical greater-than
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  t = frame.argv(3)
  f = frame.argv(4)
  if cmp(slhs, srhs) > 0:
    br.append(t)
  else:
    br.append(f)

# end ttm_gtl

def ttm_ltl(ttm, frame, br):  # ? Compare logical less-than
  slhs = frame.argv(1)
  srhs = frame.argv(2)
  t = frame.argv(3)
  f = frame.argv(4)
  if cmp(slhs, srhs) < 0:
    br.append(t)
  else:
    br.append(f)

# end ttm_ltl

# Peripheral Input/Output Operations

def ttm_ps(ttm, frame, br):  # Print a Name
  s = frame.argv(1)
  stdxx = None
  if (frame.argc() == 3):
    stdxx = frame.argv(2)
  if (stdxx != None and stdxx == "stderr"):
    target = ttm.stderr
  else:
    target = ttm.stdout
  printstring(target, s, ttm.escapec)

# end ttm_ps

def ttm_rs(ttm, frame, br):  # Read a Name
  while True:
    c = ttm.stdin.read(1)
    if (c == EOF): break
    if (c == ttm.metac):
      break
    br.put(c)
# end ttm_rs

def ttm_psr(ttm, frame, br):  # Print Name and Read
  ttm_ps(ttm, frame, br)
  ttm_rs(ttm, frame, br)
# end ttm_psr

def ttm_cm(ttm, frame, br):  # Change meta character
  smeta = frame.argv(1)
  if (len(smeta) > 0):
    if (ord(smeta[0]) > 127): ttm.fail(EASCII)
    ttm.metac = smeta[0]
# end ttm_cm

def ttm_pf(ttm, frame, br):  # Flush stdout and/or stderr
  stdxx = None
  if (frame.argc() > 1): stdxx = frame.argv(1)
  if (stdxx == None or stdxx == "stdout"):
    ttm.stdout.flush()
  if (stdxx == None or stdxx == "stderr"):
    ttm.stderr.flush()
# end ttm_pf

# Library Operations

def ttm_names(ttm, frame, br):  # Obtain all Name instance names in sorted order
  allnames = (frame.argc() > 1)

  # Collect all the names
  names = []
  for key in ttm.dictionary:
    name = ttm.dictionary[key]
    if (allnames or not name.builtin):
      names.append(key)
  # Now sort the set of names
  names.sort()
  # Return the set of names separated by commas
  first = True
  for key in names:
    if not first: br.put(',')
    br.append(key)
    first = False
# end ttm_names

def ttm_exit(ttm, frame, br):  # Return from TTM
  exitcode = 0
  ttm.flags |= FLAG_EXIT
  if (frame.argc() > 1):
    try:
      exitcode = int(frame.argv(1))
    except ValueError:
      ttm.fail(EDECIMAL)
    if (exitcode < 0): exitcode = - exitcode
  ttm.exitcode = exitcode

# end ttm_exit

# Utility Operations

def ttm_ndf(ttm, frame, br):  # Determine if a Name is Defined
  arg = frame.argv(1)
  t = frame.argv(2)
  f = frame.argv(3)
  if arg in ttm.dictionary:
    br.append(t)
  else:
    br.append(f)

# end ttm_ndf

def ttm_norm(ttm, frame, br):  # Obtain the Norm (length) of a string
  s = frame.argv(1)
  br.append(str(len(s)))

# end ttm_norm

def ttm_time(ttm, frame, br):  # Obtain time of day
  dt = time.time  # floating point in seconds
  dt *= 100  # need value to 1/100 second
  idt = int(dt)
  br.append(str(idt))

# end ttm_time

def ttm_xtime(ttm, frame, br):  # Obtain Execution Time
  xtime = time.clock() + ttm.xtimebase
  xtime *= 100  # want hundredths of a second
  br.append(str(int(xtime)))
# end ttm_xtime

def ttm_ctime(ttm, frame, br):  # Convert  ##<time> to printable string
  stod = frame.argv(1)
  try:
    tod100 = int(stod)
    todsec = tod100/100
    ftod = float(todsec)
    ct = time.ctime(ftod)
    br.append(ct)
  except ValueError:
    ttm.fail(EFORMAT)
# end ttm_ctime

def ttm_tf(ttm, frame, br):  # Turn Trace Off
  if (frame.argc() > 1):  # trace off specific name(s)
    for i in range(1, frame.argc()):
      nm = frame.argv(i)
      if nm in ttm.dictionary:
        fcn = ttm.dictionary[nm]
      else:
        ttm.fail(ENONAME)
      fcn.trace = False
  else:  # turn off all tracing
    for key in ttm.dictionary:
      fcn = ttm.dictionary[key]
      fcn.trace = False
    ttm.flags &= ~(FLAG_TRACE)

# end ttm_tf

def ttm_tn(ttm, frame, br):  # Turn Trace On
  if (frame.argc() > 1):  # trace on specific name(s)
    for i in range(1, frame.argc()):
      nm = frame.argv(i)
      if nm in ttm.dictionary:
        fcn = ttm.dictionary[nm]
      else:
        ttm.fail(ENONAME)
      fcn.trace = True
  else:  # turn on all tracing
    ttm.flags |= (FLAG_TRACE)  #trace all#

# end ttm_tn

# Functions new to this implementation

# Get ith command line argument; zero is command
def ttm_argv(ttm, frame, br):
  arg = frame.argv(1)
  try:
    index = int(arg)
  except ValueError:
    ttm.fail(EDECIMAL)
  if (index < 0 or index >= len(argoptions)):
    ttm.fail(ERANGE)
  arg = argoptions[index]
  br.append(arg)
# end ttm_argv

# Get the length of argoptions
def ttm_argc(ttm, frame, br):
  argc = len(argoptions)
  br.append(str(argc))
# end ttm_argc

def ttm_classes(ttm, frame, br):  # Obtain all character class names
  # Collect all the class names
  clnames = []
  for key in ttm.charclasses:
    clnames.append(key)
    # Now sort the set of names
  clnames.sort()
  # Return the set of names separated by commas
  first = True
  for name in clnames:
    if not first: br.put(',')
    br.append(name)
    first = False
# end ttm_classes

def ttm_lf(ttm, frame, br):  # Lock a function from being deleted
  for i in range(1, frame.argc()):
    name = frame.argv(i)
    if name in ttm.dictionary:
      fcn = ttm.dictionary[name]
      fcn.locked = True
    else:
      ttm.fail(ENONAME)

# end ttm_lf

def ttm_uf(ttm, frame, br):  # Un-Lock a function from being deleted
  for i in range(1, frame.argc()):
    name = frame.argv(i)
    if name in ttm.dictionary:
      fcn = ttm.dictionary[name]
      fcn.locked = False
    else:
      ttm.fail(ENONAME)

# end ttm_uf

def ttm_include(ttm, frame, br):  # Include text of a file
  path = frame.argv(1)
  if (len(path) == 0):
    ttm.fail(EINCLUDE)
  try:
    count = readfile(ttm,path,ttm.ba)
  except IOError:
    ttm.fail(EINCLUDE)
# end ttm_include

#Helper functions for all the ttm commands
#and subcommands

#<ttm;meta;newmetachars>

def ttm_ttm_meta(ttm, frame, br):
  arg = frame.argv(2)
  if (len(arg) != 5): ttm.fail(ETTMCMD)
  ttm.sharpc = arg[0]
  ttm.openc = arg[1]
  ttm.semic = arg[2]
  ttm.closec = arg[3]
  ttm.escapec = arg[4]

# end ttm_ttm_meta

#*
#<ttm;info;name;...>
#
def ttm_ttm_info_name(ttm, frame, br):
  for i in range(3, frame.argc()):
    arg = frame.argv(i)
    if arg in ttm.dictionary:
      entry = ttm.dictionary[arg]
    else:  # not defined#
      br.append(arg + "-,-,-\n")
      continue
    br.append(entry.name)
    if (entry.builtin):
      br.append(','+str(entry.minargs))
      if (entry.maxargs == MAXARGS):
        br.append(",*")
      else:
        br.append(','+str(entry.maxargs))
      br.put(',')
      if entry.sideeffect:
        br.put('S')
      else:
        br.put('V')
    else:  # not builtin
      br.append(",0,{0},V".format(entry.maxsegmark))
    if (not entry.builtin):
      br.append(" residual={0} body=|".format(entry.residual))
      # Walk the body checking for segment and creation marks
      for c in entry.body:
        i = ord(c)
        j = i & 0xFF
        if (iscreate(c)):
          br.append("^00")
        elif (issegmark(c)):
          br.append(("^" + SEGFORMAT).format((ord(c) & 0xFF)))
        else:
          br.put(c)
      br.put('|')
    br.put('\n')

  if DEBUG:
    ttm.stderr.write("info.name: ")
    dbgprint(ttm.stderr,br.content, '"')
    ttm.stderr.write("\n")
    ttm.stderr.flush()
  #endif
# end ttm_ttm_info_name

#*
#<ttm;info;class;...>
#
def ttm_ttm_info_class(ttm, frame, br):  # Misc. combined actions
  for i in range(3, frame.argc()):
    arg = frame.argv(i)
    if arg in ttm.charclasses:
      cl = ttm.charclasses[arg]
    else:
      ttm.fail(ENONAME)
    br.append(cl.name)
    br.put(' ')
    br.put(LBRACKET)
    if (cl.negative):
      br.put('^')
    for c in cl.characters:
      if (c == LBRACKET or c == RBRACKET):
        br.put('\\')
      br.put(c)
    br.put('n')
    # end for

  if DEBUG:
    ttm.stderr.write("info.class: ")
    dbgprint(ttm.stderr, br.content, '"')
    ttm.stderr.write("\n")
    ttm.stderr.flush()
  #endif
# end ttm_ttm_info_class

#*
#<ttm;info;XXX;...>
#

def ttm_ttm(ttm, frame, br):  # Misc. combined actions
  # Get the discriminator string
  discrim = frame.argv(1)
  if (frame.argc() >= 3 and "meta" == discrim):
    ttm_ttm_meta(ttm, frame, br)
  elif (frame.argc() >= 4 and "info" == discrim):
    discrim = frame.argv(2)
    if ("name" == discrim):
      ttm_ttm_info_name(ttm, frame, br)
    elif ("class" == discrim):
      ttm_ttm_info_class(ttm, frame, br)
    else:
      ttm.fail(ETTMCMD)
  else:
    ttm.fail(ETTMCMD)
# end ttm_ttm

##################################################
# Builtin function table
class Builtin:
  def __init__(self, name, minargs, maxargs, sv, fcn):
    self.name = name
    self.minargs = minargs
    self.maxargs = maxargs
    self.sv = sv
    self.fcn = fcn

    # end class Builtin

# TODO: fix the minargs values

# Define a subset of the original TTM functions

# Define some temporary constant
ARB = MAXARGS

builtin_orig = (
  # Dictionary Operations
  ("ap", 2, 2, "S", ttm_ap),  # Append to a string
  ("cf", 2, 2, "S", ttm_cf),  # Copy a function
  ("cr", 2, 2, "S", ttm_cr),  # Mark for creation
  ("ds", 2, 2, "S", ttm_ds),  # Define string
  ("es", 1, ARB, "S", ttm_es),  # Erase string
  ("sc", 2, 63, "SV", ttm_sc),  # Segment and count
  ("ss", 2, 2, "S", ttm_ss),  # Segment a string
  # Name Selection
  ("cc", 1, 1, "SV", ttm_cc),  # Call one character
  ("cn", 2, 2, "SV", ttm_cn),  # Call n characters
  ("sn", 2, 2, "S", ttm_sn),  # Skip n characters  #  #Batch#
  ("cp", 1, 1, "SV", ttm_cp),  # Call parameter
  ("cs", 1, 1, "SV", ttm_cs),  # Call segment
  ("isc", 4, 4, "SV", ttm_isc),  # Initial character scan
  ("rrp", 1, 1, "S", ttm_rrp),  # Reset residual pointer
  ("scn", 3, 3, "SV", ttm_scn),  # Character scan
  # Name Scanning Operations
  ("gn", 2, 2, "V", ttm_gn),  # Give n characters
  ("zlc", 1, 1, "V", ttm_zlc),  # Zero-level commas
  ("zlcp", 1, 1, "V", ttm_zlcp),  # Zero-level commas and parentheses
  ("flip", 1, 1, "V", ttm_flip),  # Flip a string  #  #Batch#
  # Character Class Operations
  ("ccl", 2, 2, "SV", ttm_ccl),  # Call class
  ("dcl", 2, 2, "S", ttm_dcl),  # Define a class
  ("dncl", 2, 2, "S", ttm_dncl),  # Define a negative class
  ("ecl", 1, ARB, "S", ttm_ecl),  # Erase a class
  ("scl", 2, 2, "S", ttm_scl),  # Skip class
  ("tcl", 4, 4, "V", ttm_tcl),  # Test class
  # Arithmetic Operations
  ("abs", 1, 1, "V", ttm_abs),  # Obtain absolute value
  ("ad", 2, ARB, "V", ttm_ad),  # Add
  ("dv", 2, 2, "V", ttm_dv),  # Divide and give quotient
  ("dvr", 2, 2, "V", ttm_dvr),  # Divide and give remainder
  ("mu", 2, ARB, "V", ttm_mu),  # Multiply
  ("su", 2, 2, "V", ttm_su),  # Substract
  # Numeric Comparisons
  ("eq", 4, 4, "V", ttm_eq),  # Compare numeric equal
  ("gt", 4, 4, "V", ttm_gt),  # Compare numeric greater-than
  ("lt", 4, 4, "V", ttm_lt),  # Compare numeric less-than
  # Logical Comparisons
  ("eq?", 4, 4, "V", ttm_eql),  # ? Compare logical equal
  ("gt?", 4, 4, "V", ttm_gtl),  # ? Compare logical greater-than
  ("lt?", 4, 4, "V", ttm_ltl),  # ? Compare logical less-than
  # Peripheral Input/Output Operations
  ("cm", 1, 1, "S", ttm_cm),  #Change Meta Character#
  ("ps", 1, 2, "S", ttm_ps),  # Print a Name
  ("psr", 1, 1, "SV", ttm_psr),  # Print Name and Read
  #if IMPLEMENTED:
  #  ("rcd",2,2,"S",ttm_rcd),  # Set to Read Prom Cards
  #endif
  ("rs", 0, 0, "V", ttm_rs),  # Read a Name
  #Formated Output Operations
  #if IMPLEMENTED:
  # ("fm",1,ARB,"S",ttm_fm),  # Format a Line or Card
  # ("tabs",1,8,"S",ttm_tabs),  # Declare Tab Positions
  # ("scc",2,2,"S",ttm_scc),  # Set Continuation Convention
  # ("icc",1,1,"S",ttm_icc),  # Insert a Control Character
  #("outb",0,3,"S",ttm_outb),  # Output the Buffer
  #endif
  # Library Operations
  #if IMPLEMENTED:
  # ("store",2,2,"S",ttm_store),  # Store a Program
  # ("delete",1,1,"S",ttm_delete),  # Delete a Program
  # ("copy",1,1,"S",ttm_copy),  # Copy a Program
  # ("show",0,1,"S",ttm_show),  # Show Program Names
  # ("libs",2,2,"S",ttm_libs),  # Declare standard qualifiers  #  #Batch#
  #endif
  ("names", 0, 1, "V", ttm_names),  # Obtain Name Names
  # Utility Operations
  #if IMPLEMENTED:
  # ("break",0,1,"S",ttm_break),  # Program Break
  #endif
  ("exit", 0, 0, "S", ttm_exit),  # Return from TTM
  ("ndf", 3, 3, "V", ttm_ndf),  # Determine if a Name is Defined
  ("norm", 1, 1, "V", ttm_norm),  # Obtain the Norm of a Name
  ("time", 0, 0, "V", ttm_time),  # Obtain time of day (modified)
  ("xtime", 0, 0, "V", ttm_xtime),  # Obtain execution time  #  #Batch#
  ("tf", 0, 0, "S", ttm_tf),  # Turn Trace Off
  ("tn", 0, 0, "S", ttm_tn),  # Turn Trace On
  ("eos", 3, 3, "V", ttm_eos),  # Test for end of string  #  #Batch#

  #if IMPLEMENTED:
  # Batch Functions
  #("insw",2,2,"S",ttm_insw),  # Control output of input monitor  #  #Batch#
  #("ttmsw",2,2,"S",ttm_ttmsw),  # Control handling of ttm programs  #  #Batch#
  # ("cd",0,0,"V",ttm_cd),  # Input one card (Batch)
  # ("cdsw",2,2,"S",ttm_cdsw),  # Control cd input (Batch)
  # ("for",0,0,"V",ttm_for),  # Input next complete fortran statement (Batch)
  # ("forsw",2,2,"S",ttm_forsw)  # Control for input (Batch)
  # ("pk",0,0,"V",ttm_pk),  # Look ahead one card (Batch)
  # ("pksw",2,2,"S",ttm_pksw),  # Control pk input (Batch)
  # ("ps",1,1,"S",ttm_ps),  # Print a string (Batch)  #Modified#
  # ("page",1,1,"S",ttm_page),  # Specify page length (Batch)
  # ("sp",1,1,"S",ttm_sp),  # Space before printing (Batch)
  # ("fm",0,ARB,"S",ttm_fm),  # Format a line or card (Batch)
  # ("tabs",1,10,"S",ttm_tabs),  # Declare tab positions (Batch)  #Modified#
  # ("scc",3,3,"S",ttm_scc),  # Set continuation convention (Batch)
  # ("fmsw",2,2,"S",ttm_fmsw),  # Control fm output (Batch)
  # ("time",0,0,"V",ttm_time),  # Obtain time of day (Batch)  #Modified#
  # ("des",1,1,"S",ttm_des),  # Define error string (Batch)
  #endif
);

# Functions new to this implementation
builtin_new = (
  ("argv", 1, 1, "V", ttm_argv),  # Get ith command line argument; 0<=i<argc
  ("argc", 0, 0, "V", ttm_argc),  # no. of command line arguments
  ("classes", 0, 0, "V", ttm_classes),  # Obtain character class Names
  ("ctime", 1, 1, "V", ttm_ctime),  # Convert time to printable string
  ("include", 1, 1, "S", ttm_include),  # Include text of a file
  ("lf", 0, ARB, "S", ttm_lf),  # Lock functions
  ("pf", 0, 1, "S", ttm_pf),  # flush stderr and/or stdout
  ("uf", 0, ARB, "S", ttm_uf),  # Unlock functions
  ("ttm", 1, ARB, "SV", ttm_ttm),  # Misc. combined actions
);


def definebuiltinfunction1(ttm, bin):
  name = bin[0]
  # Make sure we did not define builtin twice
  if name in ttm.dictionary:
    assert False, "Duplicate built-in function name"
    # create a new function object
  function = Name(ttm, name)
  function.builtin = True
  function.minargs = bin[1]
  function.maxargs = bin[2]
  function.fcn = bin[4]
  if("S" == bin[4]) :
    function.sideeffect = True
  ttm.dictionary[function.name] = function

#end definebuiltinfunction

def definebuiltinfunctions(ttm):
  for bin in builtin_orig:
    definebuiltinfunction1(ttm, bin)
  for bin in builtin_new:
    definebuiltinfunction1(ttm, bin)

#end definebuiltinfunctions

##################################################
#Startup commands: execute before
#any -e or -f arguments.
#Beware that only the defaults instance variables are defined.

startup_commands = (
  "#<ds;comment;>",
  "#<ds;def;<##<ds;name;<text>>##<ss;name;subs>>>#<ss;def;name;subs;text>",
)


def startupcommands(ttm):
  saveflags = ttm.flags
  ttm.flags &= ~FLAG_TRACE

  for cmd in startup_commands:
    ttm.active.reset()
    ttm.active.append(cmd)
    ttm.scan(ttm.active)
    ttm.active.reset()  # throw away any result
  ttm.flags = saveflags

#end startupcommands

##################################################
# Lock all the names in the dictionary

def lockup(ttm):
  for key in ttm.dictionary:
    entry = ttm.dictionary[key]
    entry.locked = True

#end lockup

##################################################
# Error reporting

ERRORDICT = {
  ENOERR: "No error",
  ENONAME: "Dictionary Name or Character Class Name Not Found",
  ENOPRIM: "Primitives Not Allowed",
  EFEWPARMS: "Too Few Parameters Given",
  EFORMAT: "Incorrect Format",
  EQUOTIENT: "Quotient Is Too Large",
  EDECIMAL: "Decimal Integer Required",
  EMANYDIGITS: "Too Many Digits",
  EMANYSEGMARKS: "Too Many Segment Marks",
  EMEMORY: "Dynamic Storage Overflow",
  EPARMROLL: "Parm Roll Overflow",
  EINPUTROLL: "Input Roll Overflow",
  #if IMPLEMENTED:
  EDUPLIBNAME: "Name Already On Library",
  ELIBNAME: "Name Not On Library",
  ELIBSPACE: "No Space On Library",
  EINITIALS: "Initials Not Allowed",
  EATTACH: "Could Not Attach",
  #endif
  EIO: "An I/O Error Occurred",
  #if IMPLEMENTED:
  ETTM: "A TTM Processing Error Occurred",
  ESTORAGE: "Error In Storage Format",
  #endif
  ENOTNEGATIVE: "Only unsigned decimal integers",
  # messages new to this implementation,
  ESTACKOVERFLOW: "Stack overflow",
  ESTACKUNDERFLOW: "Stack Underflow",
  EBUFFERSIZE: "Buffer overflow",
  EMANYINCLUDES: "Too many includes",
  EINCLUDE: "Cannot read Include file",
  ERANGE: "index out of legal range",
  EMANYPARMS: "Number of parameters greater than MAXARGS",
  EEOS: "Unexpected end of string",
  EASCII: "ASCII characters only",
  ECHAR8: "Illegal utf-8 character set",
  EUTF32: "Illegal utf-32 character set",
  ETTMCMD: "Illegal  #<ttm> command",
  ETIME: "Gettimeofday() failed",
  EEXECCOUNT: "Too many executions",
  EARITHMETIC: "Arithmetic error",
  EOTHER: "Unknown Error",
}

def errorstring(eno):
  if (eno not in ERRORDICT): eno = EOTHER
  return ERRORDICT[eno]

#end errorstring

##################################################
# Debug utility functions

if DEBUG:
  def dumpnames(ttm):
    i = 0
    for key in ttm.dictionary:
      entry = ttm.dictionary[key]
      ttm.stderr.write("{:03d} ".format(i))
      dbgprint(ttm.stderr, entry.name, '|')
      i += 1
    ttm.stderr.write("\n")

  #end dumpnames

  def dumpframe(ttm, frame):
    traceframe(ttm, frame, 1)
    ttm.stderr.write("\n")
  #end dumpframe
#endif

##################################################
# Utility functions

# Given an escaped char, return its
# actual escaped value.
# Zero indicates it should be elided

def unescapechar(c):
  if c == 'r': # '\r'
    c = '\r'
  elif c == 'n': # '\n'
    c = '\n'
  elif c == 't': # '\t'
    c = '\t'
  elif c == 'b': # '\b'
    c = '\b'
  elif c == 'f': # '\f'
    c = '\f'
  #elif c == '\n':
  #  c = 0  # elide escaped eol
  else:
    pass  # no change
  return c
#end unescapechar

def traceframe(ttm, frame, traceargs):
  if (frame.argc() == 0):
    ttm.stderr.write("#<empty frame>")
    return
  tag = EMPTY
  tag += ttm.sharpc
  if (not frame.active):
    tag += ttm.sharpc
  tag += ttm.openc
  ttm.stderr.write(tag)
  dbgprint(ttm.stderr,frame.argv(0))
  if (traceargs):
    for i in range(1, frame.argc()):
      ttm.stderr.write(ttm.semic)
      dbgprint(ttm.stderr,frame.argv(i))
  ttm.stderr.write(ttm.closec)
  ttm.stderr.flush()

# end traceframe

def trace1(ttm, depth, entering, tracing):
  if (tracing and len(ttm.stack) == 0):
    ttm.stderr.write("trace: no frame to trace\n")
    return
  frame = ttm.stack[depth]
  ttm.stderr.write("[{0:02d}] ".format(depth))
  if (tracing):
    if (entering):
      ttm.stderr.write("begin: ")
    else:
      ttm.stderr.write("end: ")
  traceframe(ttm, frame, entering)
  # Dump the contents of result if not entering
  if (not entering):
    ttm.stderr.write(" => ")
    dbgprint(ttm.stderr,frame.result.getcontents(), '"')
  ttm.stderr.write("\n")
#end trace1

# Trace a top frame in the frame stack.
def trace(ttm, entering, tracing):
  trace1(ttm, len(ttm.stack) - 1, entering, tracing)
#end trace

##################################################
# Debug Support

# Dump the stack
def dumpstack(ttm):
  ttm.stderr.write("begin stack trace:\n")
  stacklen = len(ttm.stack)
  for i in range(0,stacklen):
    trace1(ttm, i, 1, not TRACING)
  ttm.stderr.write("end stack trace:\n")
  ttm.stderr.flush()
#end dumpstack

# Given a character, return its escaped form
# including the leading escape character
# if needed
def escapechar(c, escapec, special=None):
  if special == None: special = EMPTY
  cc = u''
  if (iscreate(c)):
    return u"^00"
  elif (issegmark(c)) :
    return (u"^" + SEGFORMAT).format((ord(c) & 0xFF))
  elif (c == escapec):
    cc = escapec + escapec
  elif special.find(c) >= 0:
    # Special handling for control characters
    if iscontrol(c):
      if c == '\n': c = 'n'
      elif c == '\r': c = 'r'
      elif c == '\t': c = 't'
      elif c == '\b': c = 'b'
      elif c == '\f': c = 'f'
      else:
        c = str(ord(c))
      cc = escapec + c
  else:
    cc = c
  return cc
#end escapechar

def dbgprint(output, s, quote=None, escapec = '\\'):
  # When doing debug output, we want to convert control chars
  if (quote != None):
    output.write(quote)
    special = quote + u"\n\r\t\b\f"
  else:
    special = u"\n\r\t\b\f"
  output.write(printablestring(s, escapec, special))
  if quote != None :
    output.write(quote)
  output.flush()
#end dbgprint

##################################################
# Main() Support functions

def initglobals():
  eoptions = []
  argoptions = []
#end initglobals

def usage(msg=None):
  if (msg != None):
    sys.stderr.write(msg + "\n")
    sys.stderr.write(
      "usage: ttm "
      + "[-d string]"
      + "[-e string]"
      + "[-p programfile]"
      + "[-f inputfile]"
      + "[-o file]"
      + "[-i]"
      + "[-V]"
      + "[-q]"
      + "[-X tag=value]"
      + "[--]"
      + "[arg...]"
      + "\n")
  sys.stderr.write("\tOptions may be repeated\n")
  if (msg != None):
    sys.exit(1)
  sys.exit(0)
#end usage

def printablestring(s, escapec, special=None):
  ps = EMPTY
  if (len(s) == 0): return ps
  s = s + EOS # to avoid having check length all the time
  p = 0
  while True:
    c = s[p]
    p += 1
    if (c == EOS): break
    c = escapechar(c,escapec,special)
    ps += c    
  return ps
#end printablestring

def printstring(output, s, escapec):
  ps = printablestring(s, escapec)
  output.write(ps)
  output.flush()
#end printstring

def tagvalue(p):
  if (p == None or p[0] == NUL):
    return -1
  value = int(p)
  c = p[len(p) - 1]
  if c == NUL:
    pass
  elif c == 'm' or c == 'M':
    value *= (1 << 20)
  elif c == 'k' or c == 'K':
    value *= (1 << 10)
  return value
#end tagvalue

def setdebugflags(flagstring):
  flags = 0
  if (flagstring == None): return flags
  for c in flagstring:
    if (c == 't'): flags |= FLAG_TRACE
    elif (c == 'b'): flags |= FLAG_BARE
    elif (c == 'T'): flags |= FLAG_TESTING
  return flags
#end setdebugflags

##################################################
# IO Read functions

# Read all of the contents of a specific
# file, or stdin. Assume input is utf-8.
# Return no. of characters read.
# Raise IOError
def readfile(ttm, bb, filename, escapec):
  f = None
  count = 0
  if filename == "-":
    # Read from stdinput
    f = ttm.stdin
    doclose = False
  else:
    try:
      f = codecs.open(filename,"r","utf-8")
      doclose = True
      bb.reset()
      # Read character by character until EOF
      while True:
        c = f.read(1)
        if (c == EOF): break
        if (c == escapec):
          c = f.read(1)  # get next character
          if c == u'\n':
            continue # elide escaped newline
        bb.put(c)
        count += 1
    except IOError as e:
      ttm.stderr.write("Cannot read file: " + filename + "\n")
      raise e
    finally:
      if f != None and doclose: f.close()
    return count
#end readfile

# Read from input until the
# read characters form a
# balanced string with respect
# to the current open/close characters.
# Read past the final balancing character
# to the next end of line.
# Return 0 if the read was terminated
# by EOF. 1 otherwise.
# Raise IOError

def readbalanced(ttm, bb):
  bb.reset()
  # Read character by character until EOF; take escapes and open/close
  # into account; keep outer <...>
  depth = 0
  count = 0
  while True:
    c = ttm.stdin.read(1)
    if (c == EOF): break
    if c == ttm.escapec:
      c = ttm.stdin.read(1)
      bb.put(ttm.escapec)
      bb.put(c)
      continue
    bb.put(c)
    if (c == ttm.openc):
      depth += 1
    elif (c == ttm.closec):
      depth -= 1
      if (depth == 0): break
    count += 1
  #end while

  # skip to end of line
  while c != EOF and c != '\n':
    c = ttm.stdin.read(1)
  return (count != 0 or c != EOF)
# readbalanced

##################################################
# Main()

OPTIONS = "qd:e:f:iI:o:p:VX:-"

def main():
  global argoptions, eoptions
  buffersize = 0
  stacksize = 0
  execcount = 0
  debugargs = None
  interactive = False
  quiet = False
  outputfilename = None
  executefilename = None  # This is the ttm file to execute
  inputfilename = None  # This is data for  #<rs>
  stdoutclose = False
  stdinclose = False
  stderrclose = False
  stdin = None
  stdout = None
  stderr = None
  ttm = None

  if (len(sys.argv) == 1):
    usage(None)

  initglobals()

  # Stash argv[0]
  argoptions.append(sys.argv[0])
  try:
    opts, args = getopt.getopt(sys.argv[1:], OPTIONS, ["help", "output="])
  except getopt.GetoptError as err:
    # print help information and exit:
    print(err)  # will print something like "option -a not recognized"
    sys.exit(1)

  for opt, optarg in opts:
    if opt == "-X":
      if (optarg == None): usage("Illegal -X tag")
      c = optarg[0]
      if (c != '='): usage("Missing -X tag value")
      c = optarg[1]
      optarg = optarg[2:]
      if c == 'b':
        if (buffersize == 0):
          buffersize = tagvalue(optarg)
          if (buffersize < 0): usage("Illegal buffersize")
      elif c == 's':
        if (stacksize == 0):
          stacksize = tagvalue(optarg)
          if (stacksize < 0): usage("Illegal stacksize")
      elif c == 'x':
        if (execcount == 0):
          execcount = tagvalue(optarg)
          if (execcount < 0): usage("Illegal execcount")
      else:
        usage("Illegal -X option")
    elif opt == '-d':
      if (debugargs == None ):
        debugargs = optarg
    elif opt == '-e':
      eoptions += optarg
    elif opt == '-p':
      if (executefilename == None):
        executefilename = optarg
    elif opt == '-f':
      if (inputfilename == None):
        inputfilename = optarg
        interactive = False
    elif opt == '-o':
      if (outputfilename == None):
        outputfilename = optarg
    elif opt == '-q':
      quiet = True
    elif opt == '-V':
      print "ttm version: " + VERSION + "\n"
      sys.exit(0)
    else:
      usage("Illegal option")
      # end for

  # Collect any args for  #<arg>
  if (len(args) > 0):
    argoptions += args

  # Complain if interactive and output file name specified
  if (outputfilename != None and interactive):
    sys.stderr.write("Interactive is illegal if output file specified\n")
    sys.exit(1)

  if (buffersize < DFALTBUFFERSIZE):
    buffersize = DFALTBUFFERSIZE
  if (stacksize < DFALTSTACKSIZE):
    stacksize = DFALTSTACKSIZE
  if (execcount < DFALTEXECCOUNT):
    execcount = DFALTEXECCOUNT

  if (stdout == None):
    stdout = UTF8Writer(sys.stdout)
    stdoutclose = False
  else:
    try:
      stdout = codecs.open(outputfilename, "w", "utf-8")
    except IOError:
      sys.stderr.write("Output file is not writable: " + outputfilename + "\n")
      sys.exit(1)
    stdoutclose = True

  if (inputfilename == None):
    stdin = UTF8Reader(sys.stdin)
    stdinclose = False
  else:
    try:
      stdin = codecs.open(inputfilename, "r", "utf-8")
    except IOError:
      sys.stderr.write("-f file is not readable: " + inputfilename + "\n")
      sys.exit(1)
    stdinclose = True

  # Define flags
  flags = setdebugflags(debugargs)

  # Create the ttm state
  ttm = TTM(buffersize, stacksize, execcount)
  ttm.flags |= flags
  ttm.stdout = stdout
  ttm.stdoutclose = stdoutclose
  ttm.stdin = stdin
  ttm.stdinclose = stdinclose
  if testMark(ttm.flags,FLAG_TESTING):
    ttm.stderr = ttm.stdout
  else:
    ttm.stderr = UTF8Writer(sys.stderr)
  ttm.stderrclose = False  

  definebuiltinfunctions(ttm)
  
  if not testMark(ttm.flags,FLAG_BARE):
    startupcommands(ttm)
  # Lock up all the currently defined functions
  lockup(ttm)

  # Execute the -e strings in turn
  exiting = False
  for eopt in eoptions:
    ttm.active.reset()
    ttm.active.append(eopt)
    ttm.scan()
    if ((ttm.flags & FLAG_EXIT) != 0):
      exiting = True
      break

  # Now execute the executefile, if any
  if (not exiting and executefilename != None):
    try:
      readfile(ttm, ttm.active, executefilename, ttm.escapec)
    except IOError:
      ttm.fail(EIO)
    value = ttm.scan(ttm.active)
    # Dump any output left in the passive buffer
    if not quiet:
      printstring(ttm.stdout, value, ttm.escapec)
    if ((ttm.flags & FLAG_EXIT) != 0):
      exiting = True

  # If interactive, start read-eval loop
  if (not exiting and interactive):
    while True:
      ttm.active.reset()
      if (not readbalanced(ttm,ttm.active)): break
      value = ttm.scan()
      printstring(ttm.stdout, value, ttm.escapec)
      if ((ttm.flags & FLAG_EXIT) != 0):
        exiting = True
        break

#done:
  exitcode = ttm.exitcode

  # cleanup
  if (not ttm.stdoutclose): ttm.stdout.close()
  if (not ttm.stderrclose): ttm.stderr.close()
  if (not ttm.stdinclose): ttm.stdin.close()
  # the ttm reference in the dictionary entries
  # is a potential circular reference
  for key in ttm.dictionary:
    entry = ttm.dictionary[key]
    entry.ttm = None

  sys.exit(exitcode)

# end Main

if __name__ == "__main__":
  main()

