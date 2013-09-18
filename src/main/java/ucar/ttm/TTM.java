/**
 This software is released under the terms of the Apache License version 2.
 For details of the license, see http://www.apache.org/licenses/LICENSE-2.0.
 */

package ucar.ttm;

import java.io.*;
import java.nio.charset.Charset;
import java.util.*;
import java.text.SimpleDateFormat;

/**
 * The critical problem for a Java
 * implementation is how to deal
 * with character pointers.
 * The C version has the option of
 * using direct pointers into
 * the buffers. Java does not have that option.
 * On the other hand is we use, say, Java
 * StringBuffers or Strings, then we have to do
 * a lot of String allocations and that is
 * costly (although that assertion is untested).
 * The other option is to use a big char buffer
 * and using integer indices into that buffer.
 * <p/>
 * However, the cognitive burden in using integer indices
 * is very high. So, ignoring the performance issues,
 * this Java implementation uses Strings, StringBuffers
 * (StringBuilder actually) and such.
 * <p/>
 * Another issue involves the character set. Since Java
 * so naturally supports UTF-16, we adhere to that
 * and restrict ourselves to UTF-16 and use the
 * existing Java char and Character types.
 * <p/>
 * Note also that it is assumed that all C string constants
 * in this file are restricted to US-ASCII, which is a subset
 * of both UTF-8.
 * <p/>
 * Use is also made of the UTF-16 so-called "private-use" characters
 * -- see "http://en.wikipedia.org/wiki/Private_Use_(Unicode)" --
 * to encode segment and creation marks.
 * <p/>
 * Note also that it would be theoretically possible to use
 * Java reflection to dynamically load new ttm builtin functions.
 * <p/>
 * ----------------------------------------
 * Differences (vis-a-vis C implementation)
 * ----------------------------------------
 * 1. ISO8859 is not supported.
 * 2. Command line options may not be repeated
 * (due to using the Java -D flag).
 */

public class TTM
{

//////////////////////////////////////////////////
// Constants

    static final String ARGV0 = "TTM"; //pretend

    static final String VERSION = "1.0";

    static final boolean DEBUG = false;

    static final Charset UTF8 = Charset.forName("UTF-8");

    static final Calendar CALENDAR = Calendar.getInstance();

    // Assign special meaning to some otherwise illegal utf-16 character values
    static final char SEGMARK = '\uE000';
    static final char CREATE = '\uF000';

    // Limits
    static final int MAXMARKS = 62;
    static final int MAXARGS = 63;
    static final int ARB = MAXARGS;
    static final int MAXEOPTIONS = 1024;
    static final int MAXINTCHARS = 32;

    // Character mnemonics
    static final char NUL = '\u0000';
    static final char COMMA = ',';
    static final char LPAREN = '(';
    static final char RPAREN = ')';
    static final char LBRACKET = '[';
    static final char RBRACKET = ']';
    static final char EOS = NUL;

    static final int EOF = -1;

// The following are enforced mostly in order
// to prevent run-away computations.
// These are the defaults and can be overridden

    static final int DFALTBUFFERSIZE = (1 << 24);
    static final int DFALTSTACKSIZE = 256;
    static final int DFALTEXECCOUNT = (1 << 16);

    static final int CONTEXTLEN = 20;

    static final int CREATELEN = 4; //# of digits for a create mark

    static final int HASHSIZE = 128;

    // Other Mnemonics
    static final boolean NESTED = true;
    static final boolean KEEPESCAPE = true;
    static final boolean TOSTRING = true;
    static final boolean TRACING = true;

    // TTM Flags */
    static final int FLAG_EXIT = 1;
    static final int FLAG_TRACE = 2;
    static final int FLAG_BARE = 4; // Do not do startup initializations

//////////////////////////////////////////////////

    // Error Numbers
    static enum ERR
    {
        ENOERR(0),          // No error; for completeness
        ENONAME(1),         // Dictionary Name or Character Class Name Not Found
        ENOPRIM(2),         // Primitives Not Allowed
        EFEWPARMS(3),       // Too Few Parameters Given
        EFORMAT(4),         // Incorrect Format
        EQUOTIENT(5),       // Quotient Is Too Large
        EDECIMAL(6),        // Decimal Integer Required
        EMANYDIGITS(7),     // Too Many Digits
        EMANYSEGMARKS(8),   // Too Many Segment Marks
        EMEMORY(9),         // Dynamic Storage Overflow
        EPARMROLL(10),      // Parm Roll Overflow
        EINPUTROLL(11),     // Input Roll Overflow
        /*#ifdef IMPLEMENTED
EDUPLIBNAME     = 12,     // Name Already On Library
ELIBNAME        = 13,     // Name Not On Library
ELIBSPACE       = 14,     // No Space On Library
EINITIALS       = 15,     // Initials Not Allowed
EATTACH         = 16,     // Could Not Attach
#endif*/
        EIO(17),            // An I/O Error Occurred
        /*#ifdef IMPLEMENTED
ETTM            = 18,     // A TTM Processing Error Occurred
ESTORAGE        = 19,     // Error In Storage Format
#endif*/
        ENOTNEGATIVE(20),      //Integer value must be greater than or equal to zero
        // Error messages new to this implementation
        ESTACKOVERFLOW(30),
        ESTACKUNDERFLOW(31),
        EBUFFERSIZE(32),    // Buffer overflow
        EMANYINCLUDES(33),  // Too many includes (obsolete)
        EINCLUDE(34),       // Cannot read Include file
        ERANGE(35),         // index out of legal range
        EMANYPARMS(36),     // #parameters > MAXARGS
        EEOS(37),           // Unexpected end of string
        EASCII(38),         // ASCII characters only
        ECHAR8(39),         // Illegal 8-bit character set value
        EUTF32(40),         // Illegal utf-32 character set
        ETTMCMD(41),        // Illegal #<ttm> command
        ETIME(42),          // gettimeofday failed
        EEXECCOUNT(43),     // too many execution calls
        EOTHER(99);         // Default case
        private int code;

        ERR(int i)
        {
            this.code = i;
        }
    }

    ;

//////////////////////////////////////////////////
// "inline" functions

    // Macros to set/clear/test these marks
    static int setMark(int w, int mark)
    {
        return (w | mark);
    }

    static int clearMark(int w, int mark)
    {
        return (w & ~mark);
    }

    static boolean testMark(int w, int mark)
    {
        return ((w & 0xFF00) == mark);
    }

    boolean isescape(char c)
    {
        return ((c) == this.escapec);
    }

    static boolean issegmark(char c)
    {
        return testMark(c, SEGMARK);
    }

    static boolean iscreate(char c)
    {
        return testMark(c, CREATE);
    }

    static boolean ismark(char c)
    {
        return issegmark(c) || iscreate(c);
    }

    static boolean iscontrol(char c)
    {
        return (c < ' ' || c == 127);
    }

    static boolean iswhitespace(char c)
    {
        return (iscontrol(c) || c == ' ');
    }

    static boolean isdec(char c)
    {
        return (c >= '0' && c <= '9');
    }

    static boolean ishex(char c)
    {
        return (c >= '0' && c <= '9')
            || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F');
    }

    static int fromhex(char c)
    {
        return (c >= '0' && c <= '9'
            ? (c - '0')
            : (c >= 'a' && c <= 'f'
            ? ((c - 'a') + 10)
            : (c >= 'A' && c <= 'F'
            ? ((c - 'a') + 10)
            : -1)));
    }

//////////////////////////////////////////////////
// Type declarations

    /**
     * Define the limits to prevent
     * run-away computations.
     */
    static class Limits
    {
        long buffersize;
        long stacksize;
        long execcount;
    }

    ;

    /**
     * Java does not have functions as first class
     * objects, so use anonymous classes to simulate.
     */
    static interface TTMFCN
    {
        void invoke(Frame frame, StringBuilder result) throws Fatal;
    }

    /**
     * Define a ttm frame
     */

    static class Frame
    {
        String[] argv = new String[MAXARGS + 1];
        int argc = 0;
        boolean active = true; // true => # false => ##
        StringBuilder result = null;

        public String toString()
        {
            StringBuilder buf = new StringBuilder();
            buf.append(active?"#<":"##<");
            for(int i=0;i<argc;i++) {
                if(i > 0) buf.append(";");
                buf.append(argv[i]);
            }
            buf.append(">");
            return buf.toString();
        }
    }



    /**
     * Define active buffer class
     */
    static class Active
    {
        StringBuilder content = new StringBuilder();
        int pos = 0;

        int length()
        {
            return content.length();
        }

        int pos()
        {
            return pos;
        }

        void reset()
        {
            pos = 0;
        }

        void skip(int n)
        {
            pos += n;
        }

        void skip()
        {
            skip(1);
        }

        char next()
        {
            if(pos >= content.length()) return EOS;
            return content.charAt(pos++);
        }

        char peek(int i)
        {
            if(pos + i >= content.length()) return EOS;
            return content.charAt(pos + i);
        }

        char peek()
        {
            return peek(0);
        }

        void setContent(String s)
        {
            pos = 0;
            content.setLength(0);
            content.append(s);
        }

        void add(StringBuilder sb)
        {
            content.insert(pos, sb.toString());
        }

        public String toString()
        {
            return content.substring(0,pos) + "|" + content.substring(pos,content.length());
        }
    }

    /**
     * Name Storage and the Dictionary
     * <p/>
     * If you add field to this, you need
     * to modify especially ttm_ds
     */
    static class Name
    {
        String name = null;
        boolean trace = false;
        boolean locked = false;
        boolean builtin = false;
        boolean sideeffect = false; // side effect only
        int minargs = 0;
        int maxargs = 0;
        int residual = 0;
        int maxsegmark = 0; // highest segment mark number in use in this string
        TTMFCN fcn = null; // builtin == true
        String body = null; // builtin == false
    }

    ;

    /**
     * Character Classes  and the Charclass table
     */

    static class Charclass
    {
        String name;
        String characters;
        boolean negative;
    }

    ;

//////////////////////////////////////////////////
// Class variables

    // Class -Ddebug flags
    static boolean testing = false; // true=>make stderr and stdout same
    static boolean bare = false; // true=>suppress any startup

//////////////////////////////////////////////////
// Class methods

//////////////////////////////////////////////////
// Instance variables

    int flags;
    int exitcode;
    int crcounter; // for cr marks
    char sharpc; // sharp-like char
    char openc; // <-like char
    char closec; // >-like char
    char semic; // ;-like char
    char escapec; // escape-like char
    char metac; // read eof char

    Stack<Frame> stack;
    Map<String, Name> dictionary;
    Map<String, Charclass> charclasses;

    // Get the startup time to determine xtime
    long starttime;

    PrintWriter stdout;
    PrintWriter stderr;
    Reader stdin;


//////////////////////////////////////////////////
// Command line values

    Limits limits;
    List<String> argv = new ArrayList<String>();

    PrintWriter output;
    boolean isstdout;
    Reader rsinput;
    boolean isstdin;

//////////////////////////////////////////////////
// Constructor(s)

    public TTM()
    {
        this.flags = 0;
        this.exitcode = 0;
        this.sharpc = '#';
        this.openc = '<';
        this.closec = '>';
        this.semic = ';';
        this.escapec = '\\';
        this.metac = '\n';
        this.stack = new Stack<Frame>();
        this.dictionary = new HashMap<String, Name>();
        this.charclasses = new HashMap<String, Charclass>();
        this.starttime = System.nanoTime();
        this.stdout = new PrintWriter(
            new OutputStreamWriter(System.out, UTF8), true);
        if(testing)
            this.stderr = stdout;
        else
            this.stderr = new PrintWriter(new OutputStreamWriter(System.err, UTF8), true);
        this.stdin = new InputStreamReader(System.in, UTF8);
        if(DEBUG)
            this.flags |= FLAG_TRACE;
        setLimits(DFALTBUFFERSIZE, DFALTSTACKSIZE, DFALTEXECCOUNT);
        this.argv = new ArrayList<String>();
        this.output = this.stdout;
        this.isstdout = true;
        this.rsinput = this.stdin;
        this.isstdin = true;
        defineBuiltinFunctions();
        if(!bare)
            startupcommands();
        lockup();
    }

//////////////////////////////////////////////////
// Cleanup

    public void
    close()
    {
    }

//////////////////////////////////////////////////
// Accessors

    public Limits getLimits()
    {
        return limits;
    }

    public void setLimits(Limits l)
    {
        this.limits = l;
    }

    public void setLimits(long buffersize, long stacksize, long execcount)
    {
        if(buffersize < DFALTBUFFERSIZE) buffersize = DFALTBUFFERSIZE;
        if(stacksize < DFALTSTACKSIZE) stacksize = DFALTSTACKSIZE;
        if(execcount < DFALTEXECCOUNT) execcount = DFALTEXECCOUNT;
        Limits limits = new Limits();
        limits.buffersize = buffersize;
        limits.stacksize = stacksize;
        limits.execcount = execcount;
        setLimits(limits);
    }

    public List<String> getArgv()
    {
        return this.argv;
    }

    public void addArgv(String arg)
    {
        if(!argv.contains(arg)) this.argv.add(arg);
    }

    public PrintWriter getOutput()
    {
        return this.output;
    }

    public void setOutput(PrintWriter pw, boolean isstdout)
    {
        this.output = pw;
        this.isstdout = isstdout;
    }

    public Reader getInput()
    {
        return this.rsinput;
    }

    public void setInput(Reader r, boolean isstdin)
    {
        this.rsinput = r;
        this.isstdin = isstdin;
    }

    public boolean isstdout()
    {
        return this.isstdout;
    }

    public boolean isstdin()
    {
        return this.isstdin;
    }

    // Misc.
    public int getExitcode()
    {
        return this.exitcode;
    }

    public int getFlags()
    {
        return this.flags;
    }

    public PrintWriter getStdout()
    {
        return this.stdout;
    }

    public PrintWriter getStderr()
    {
        return this.stderr;
    }

    public Reader getStdin()
    {
        return this.stdin;
    }

    // Test flags
    public boolean testFlag(int flag)
    {
        return (flags & flag) != 0;
    }


//////////////////////////////////////////////////
/**
 External API
 */

    /**
     * Read-Eval loop
     *
     * @throws Fatal
     */

    public void
    evalloop()
    {
        StringBuilder text = new StringBuilder();
        boolean eof = false;
        String result = null;
        for(;;) {
            text.setLength(0);
            eof = readbalanced(text);
            result = scan(text.toString());
            if(testFlag(FLAG_EXIT) || eof)
                break;
            printstring(this.output, result);
        }
    }

    /**
     * Scan a given text string
     *
     * @param text the text to scan
     * @return the resulting output
     * @throws Fatal
     */

    public String
    scan(String text)
    {
        Active active = new Active();
        active.setContent(text);
        String result = scan(active);
        return result;
    }

//////////////////////////////////////////////////
// Manage the frame stack

    Frame
    pushFrame()
    {
        if(stack.size() >= limits.stacksize)
            fail(ERR.ESTACKOVERFLOW);
        Frame frame = new Frame();
        stack.push(frame);
        return frame;
    }

    Frame
    popFrame()
    {
        if(stack.size() == 0)
            fail(ERR.ESTACKUNDERFLOW);
        Frame frame = stack.pop();
        return frame;
    }

//////////////////////////////////////////////////
// Manage a buffer

    String
    scan(Active active)
    {
        int index;
        char c;
        StringBuilder passive = new StringBuilder();
        for(;;) {
            c = active.peek(); // NOTE that we do not bump here
            if(c == EOS) { // End of buffer
                break;
            } else if(isescape(c)) {
                active.skip(); // skip the escape
                passive.append(active.next());
            } else if(c == sharpc) {// Start of call?
                if(active.peek(1) == openc
                    || (active.peek(1) == sharpc
                    && active.peek(2) == openc)) {
                    // It is a real call
                    boolean isactive = (active.peek(1) == openc);
                    exec(active, passive);
                    if((flags & FLAG_EXIT) != 0) return passive.toString();
                } else {// not a call; just pass the # along passively
                    passive.append(c);
                    active.skip();
                }
            } else if(c == openc) { // Start of <...> escaping
                // skip the leading lbracket
                int depth = 1;
                active.skip();
                for(;;) {
                    c = active.peek();
                    if(c == EOS) fail(ERR.EEOS); // Unexpected EOF
                    passive.append(c);
                    active.skip();
                    if(isescape(c)) {
                        passive.append(active.next());
                    } else if(c == openc) {
                        depth++;
                    } else if(c == closec) {
                        if(--depth == 0) {
                            break;
                        } // we are done
                    } // else keep moving
                }/*<...> for*/
            } else { // non-signficant character
                passive.append(c);
                active.skip();
            }
        } /*scan for*/

        return passive.toString();
    }

    void
    exec(Active active, StringBuilder passive)
    {
        Frame frame;
        Name fcn;

        if(limits.execcount-- <= 0)
            fail(ERR.EEXECCOUNT);
        frame = pushFrame();
        // Skip to the start of the function name
        if(active.peek(1) == openc) {
            active.skip(2);
            frame.active = true;
        } else {
            active.skip(3);
            frame.active = false;
        }
        // Parse and store relevant pointers into frame.
        parsecall(frame, active);
        if((flags & FLAG_EXIT) != 0) {
            popFrame();
            return;
        }

        // Now execute this function
        if(frame.argc == 0) fail(ERR.ENONAME);
        if(frame.argv[0].length() == 0) fail(ERR.ENONAME);

        // Locate the function to execute
        String fcnname = frame.argv[0];
        fcn = dictionary.get(fcnname);
        if(fcn == null) fail(ERR.ENONAME);
        if(fcn.minargs > (frame.argc - 1)) // -1 to account for function name
            fail(ERR.EFEWPARMS);
        if((flags & FLAG_TRACE) != 0 || fcn.trace)
            trace(true, TRACING);
        if(!fcn.sideeffect)
            frame.result = new StringBuilder();
        if(fcn.builtin) {
            fcn.fcn.invoke(frame, frame.result);
        } else { // invoke the pseudo function "call"
            call(frame, fcn.body);
        }
        if(DEBUG) {
            stderr.printf("result: ");
            dbgprint(frame.result, '|');
            stderr.printf("\n");
        }
        if((flags & FLAG_TRACE) != 0 || fcn.trace)
            trace(false, TRACING);
        // Insert the result in the right place
        if(frame.active) {
            if(frame.result != null)
                active.add(frame.result);
        } else {
            if(frame.result != null)
                passive.append(frame.result);
        }
        popFrame();
    }

    /**
     * Construct a frame; leave active.active pointing just
     * past the call.
     *
     * @param frame  the frame to fill with arguments
     * @param active the buffer from which to take the arguments
     */
    void
    parsecall(Frame frame, Active active)
    {
        int depth;
        boolean done;
        char c;
        StringBuilder tmp = new StringBuilder();

        done = false;
        do {
            int pos = active.pos();
            tmp.setLength(0);
            while(!done) {
                c = active.peek(); // Note that we do not bump here
                if(c == EOS) fail(ERR.EEOS); // Unexpected end of buffer
                if(isescape(c)) {
                    active.skip();
                    tmp.append(active.next());
                } else if(c == semic || c == closec) {
                    // End of an argument; capture the argument
                    String arg = tmp.toString();
                    if(DEBUG) {
                        stderr.printf("parsecall: argv[%d]=", frame.argc);
                        dbgprint(arg, '|');
                        stderr.printf("\n");
                    }
                    active.skip(); // skip the semi or close
                    // move to next arg
                    frame.argv[frame.argc++] = arg;
                    if(c == closec) done = true;
                    else if(frame.argc >= MAXARGS) fail(ERR.EMANYPARMS);
                    tmp.setLength(0);
                } else if(c == sharpc) {
                    // check for call within call
                    if(active.peek(1) == openc
                        || (active.peek(1) == sharpc
                        && active.peek(2) == openc)) {
                        // Recurse to compute inner call
                        exec(active, tmp);
                        if((flags & FLAG_EXIT) != 0) return;
                    }
                } else if(c == openc) {// <...> nested brackets
                    active.next(); // skip leading lbracket
                    depth = 1;
                    for(;;) {
                        c = active.peek();
                        if(c == NUL) fail(ERR.EEOS); // Unexpected EOF
                        if(isescape(c)) {
                            tmp.append((char) c);
                            tmp.append(active.next());
                        } else if(c == openc) {
                            tmp.append((char) c);
                            active.skip();
                            depth++;
                        } else if(c == closec) {
                            depth--;
                            active.next();
                            if(depth == 0) break; // we are done
                            tmp.append((char) c);
                        } else {
                            tmp.append(active.next());
                        }
                    }/*<...> for*/
                } else {
                    // keep moving
                    tmp.append((char) c);
                    active.next();
                }
            } // collect argument for
        } while(!done);
    }

//////////////////////////////////////////////////

    /**
     * Execute a non-builtin function
     *
     * @param frame the frame of arguments
     * @param body  the non-builtin function body text
     */

    void
    call(Frame frame, String body)
    {
        String crmark = null;
        assert (frame.result != null);
        // Fill in the body and place in result
        for(int p = 0;p < body.length();p++) {
            char c = body.charAt(p);
            if(testMark(c, SEGMARK)) {
                int segindex = (int) (c & 0xFF);
                if(segindex < frame.argc) {
                    String arg = frame.argv[segindex];
                    frame.result.append(arg);
                } // else treat as null string
            } else if(c == CREATE) {
	        if(crmark == null) {
                    // construct the create mark text
                    crcounter++;
                    crmark = String.format("%d", crcounter);
                    // make crmark be CREATELEN characters long
                    while(crmark.length() < CREATELEN) crmark = '0' + crmark;
                }
                frame.result.append(crmark);
            } else
                frame.result.append(c);
        }
    }

//////////////////////////////////////////////////
// Built-in Support Procedures

    /**
     * Print a string to the specified output.
     * If a segment mark or create mark is encountered,
     * it is output in a special readable form.
     *
     * @param output the stream to which the string is printed
     * @param s      the string to output
     */

    void
    printstring(PrintWriter output, String s)
    {
        int slen = s.length();
        if(s == null || slen == 0) return;
        s = s + EOS;

        for(int i = 0;i < slen;i++) {
            char c = s.charAt(i);
            if(isescape(c)) {
                i++;
                c = s.charAt(i);
                c = convertEscapeChar(c);
            }
            if(c != EOS) {
                if(ismark(c)) {
                    String smark;
                    if(iscreate(c))
                        smark = "^00";
                    else {// segmark
                        int mark = ((int) c) & 0xFF;
                        smark = String.format("^%02d", mark);
                    }
                    fputs(smark, output);
                } else
                    fputc(c, output);
            }
        }
        output.flush();
    }

//////////////////////////////////////////////////
// Builtin functions

// Original TTM functions

    // Dictionary Operations
    final TTMFCN ttm_ap = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Append to a string
        {
            Name str = dictionary.get(frame.argv[1]);
            if(str == null) {// Define the string
                ttm_ds.invoke(frame, result);
                return;
            }
            if(str.builtin) fail(ERR.ENOPRIM);
            str.body += frame.argv[2];
            str.residual = str.body.length();
        }
    }; //ttm_ap

    /**
     * The semantics of #<cf>
     * have been changed. See html
     */

    final TTMFCN ttm_cf = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Copy a function
        {
            String newname = frame.argv[1];
            String oldname = frame.argv[2];
            Name newstr = dictionary.get(newname);
            Name oldstr = dictionary.get(oldname);
            if(oldstr == null)
                fail(ERR.ENONAME);
            if(newstr == null) {
                // create a new string object
                newstr = new Name();
                newstr.name = newname;
                dictionary.put(newstr.name, newstr);
            }

            newstr.name = newname;
            newstr.trace = oldstr.trace;
            newstr.locked = oldstr.locked;
            newstr.builtin = oldstr.builtin;
            newstr.sideeffect = oldstr.sideeffect;
            newstr.minargs = oldstr.minargs;
            newstr.maxargs = oldstr.maxargs;
            newstr.residual = oldstr.residual;
            newstr.maxsegmark = oldstr.maxsegmark;
            newstr.fcn = oldstr.fcn;
            newstr.body = oldstr.body;
        }
    }; //ttm_cf

    final TTMFCN ttm_cr = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Mark for creation
        {
            Name str = dictionary.get(frame.argv[1]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            String body = str.body;
            String crstring = frame.argv[2];
            if(crstring.length() > 0) { // search only if possible success
                int p;
                // Search for occurrences of arg
                p = str.residual;
                for(;;) {
                    int q = body.indexOf(crstring, p);
                    if(q < 0) break;
                    p = q + crstring.length();
                    // we have a match, replace match by a create marker
                    body = body.substring(0, q)
                        + CREATE
                        + body.substring(p, body.length());
                }
                str.body = body;
            }
        }
    }; //ttm_cr

    final TTMFCN ttm_ds = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result)
        {
            String fname = frame.argv[1];
            Name str = dictionary.get(frame.argv[1]);
            if(str == null) {
                // create a new string object
                str = new Name();
                str.name = frame.argv[1];
                dictionary.put(str.name, str);
            } else {
                // reset as needed
                str.builtin = false;
                str.minargs = 0;
                str.maxargs = 0;
                str.residual = 0;
                str.maxsegmark = 0;
                str.fcn = null;
                str.body = null;
            }
            str.body = frame.argv[2];
        }
    }; //ttm_ds

    final TTMFCN ttm_es = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Erase string
        {
            for(int i = 1;i < frame.argc;i++) {
                String strname = frame.argv[i];
                Name str = dictionary.get(strname);
                if(str != null && !str.locked) {
                    dictionary.remove(strname);
                }
            }
        }
    }; //ttm_es

    // Helper function for #<sc> and #<ss>
    int
    ttm_ss0(Frame frame)
    {
        Name str = dictionary.get(frame.argv[1]);
        if(str == null)
            fail(ERR.ENONAME);
        if(str.builtin)
            fail(ERR.ENOPRIM);

        String body = str.body;
        int bodylen = str.body.length();
        if(str.residual >= bodylen)
            return 0; // no substitution possible
        int segcount = 0;
        int startseg = str.maxsegmark;
        int startp = str.residual;
        for(int i = 2;i < frame.argc;i++) {
            String arg = frame.argv[i];
            int arglen = arg.length();
            if(arglen > 0) { // search only if possible success
                boolean found = false;
                int p = startp;
                for(;;) {
                    int q = body.indexOf(arg, p);
                    if(q < 0) break;
                    if(!found) {// first match
                        startseg++;
                        found = true;
                    }
                    p = q + arg.length();
                    // we have a match, replace match by a segment marker
                    char mark = (char) (SEGMARK | startseg);
                    body = body.substring(0, q)
                        + mark
                        + body.substring(p, body.length());
                    segcount++;
                }
            }
        }
        str.body = body;
        str.maxsegmark = startseg;
        return segcount;
    }

    final TTMFCN ttm_sc = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Segment and count
        {
            int nsegs = ttm_ss0(frame);
            // Insert into result
            result.append(String.format("%d", nsegs));
        }
    }; //ttm_sc

    final TTMFCN ttm_ss = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Segment and count
        {
            ttm_ss0(frame);
        }
    }; //ttm_ss

// Name Selection

    final TTMFCN ttm_cc = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Call one character
        {
            Name str = dictionary.get(frame.argv[1]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            // Check for pointing at trailing NUL
            if(str.residual < str.body.length()) {
                char c = str.body.charAt(str.residual);
                result.append(c);
                str.residual++;
            }
        }
    }; //ttm_cc

    final TTMFCN ttm_cn = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Call n characters
        {
            Name str = dictionary.get(frame.argv[2]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            // Get number of characters to extract
            int n = (int) toInt64(frame.argv[1]);
            if(n < 0) fail(ERR.ENOTNEGATIVE);

            // See if we have enough space
            int bodylen = str.body.length();
            int avail;
            if(str.residual < bodylen)
                avail = (bodylen - str.residual);
            else
                avail = 0;

            if(n == 0 || avail == 0) return;
            if(avail < n) n = avail; // return what is available

            // We want n characters starting at residual
            int startn = str.residual;

            // ok, copy n characters from startn into the return buffer
            result.append(str.body.substring(startn, startn + n));
            // increment residual
            str.residual += n;
            return;
        }
    }; //ttm_cm

    final TTMFCN ttm_cp = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Call parameter
        {
            Name str = dictionary.get(frame.argv[1]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            int bodylen = str.body.length();
            int rp0 = str.residual;
            int rp = rp0;
            int depth = 0;
            char c = 0;
            String body = str.body + EOS; // avoid need to count
            for(;(c = body.charAt(rp)) != EOS;rp++) {
                if(c == semic) {
                    if(depth == 0) break; // reached unnested semicolon
                } else if(c == openc) {
                    depth++;
                } else if(c == closec) {
                    depth--;
                }
            }
            int delta = rp - rp0;
            if(delta > 0)
                result.append(str.body.substring(rp0, rp));
            str.residual += delta;
            if(c != EOS) str.residual++;
        }
    }; //ttm_cp

    final TTMFCN ttm_cs = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Call segment
        {
            Name str = dictionary.get(frame.argv[1]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            // Locate the next segment mark
            // Unclear if create marks also qualify; assume yes
            int bodylen = str.body.length();
            int rp0 = str.residual;
            int rp = rp0;
            char c = 0;
            for(;rp < bodylen;rp++) {
                c = str.body.charAt(rp);
                if(c == EOS || testMark(c, SEGMARK) || testMark(c, CREATE))
                    break;
            }
            int delta = (rp - rp0);
            if(delta > 0)
                result.append(str.body.substring(rp0, rp));
            // set residual pointer correctly
            str.residual += delta;
            if(c != EOS) str.residual++;
        }
    }; //ttm_cs

    final TTMFCN ttm_isc = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Initial character scan moves residual pointer
        {
            Name str = dictionary.get(frame.argv[2]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            String arg = frame.argv[1];
            int arglen = arg.length();
            String t = frame.argv[3];
            String f = frame.argv[4];

            // check for initial string match
            if(str.body.startsWith(arg, str.residual)) {
                result.append(t);
                str.residual += arglen;
                int bodylen = str.body.length();
                if(str.residual > bodylen) str.residual = bodylen;
            } else
                result.append(f);
        }
    }; //ttm_isc

    final TTMFCN ttm_rrp = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Reset residual pointer
        {
            Name str = dictionary.get(frame.argv[1]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);
            str.residual = 0;
        }
    }; //ttm_rp

    final TTMFCN ttm_scn = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Character scan
        {
            Name str = dictionary.get(frame.argv[2]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            String arg = frame.argv[1];
            int arglen = arg.length();
            String f = frame.argv[3];

            // check for sub string match
            int p0 = str.residual;
            int q = str.body.indexOf(arg, p0);
            if(q >= 0) {
                // return characters from the residual pointer to the
                // beginning of the first occurrence of arg
                result.append(str.body.substring(p0, q));
                if(q == p0) {
                    str.residual += arglen;
                    if(str.residual > str.body.length())
                        str.residual = str.body.length();
                }
                return;
            } else
                result.append(f);
        }
    }; //ttm_scn

    final TTMFCN ttm_sn = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Skip n characters
        {
            Name str = dictionary.get(frame.argv[2]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            // Get number of characters to skip
            long n = toInt64(frame.argv[1]);
            if(n < 0) fail(ERR.ENOTNEGATIVE);

            str.residual += n;
            int bodylen = str.body.length();
            if(str.residual > bodylen) str.residual = bodylen;
        }
    }; //ttm_sn

    final TTMFCN ttm_eos = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Test for end of string
        {
            Name str = dictionary.get(frame.argv[1]);
            if(str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            int bodylen = str.body.length();
            String t = frame.argv[2];
            String f = frame.argv[3];
            String tf = (str.residual >= bodylen ? t : f);
            result.append(tf);
        }
    }; //ttm_eos

// Name Scanning Operations

    final TTMFCN ttm_gn = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Give n characters from argument string
        {
            String snum = frame.argv[1];
            String s = frame.argv[2];
            long slen = s.length();
            int startp = 0;

            long num = toInt64(snum);
            if(num > 0) {
                if(slen < num) num = slen;
                startp = 0;
            } else if(num < 0) {
                num = -num;
                startp = (int) num;
                num = (slen - num);
            }
            if(num != 0) {
                result.append(s.substring(startp, (int) (startp + num)));
            }
        }
    }; //ttm_gn

    final TTMFCN ttm_zlc = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Zero-level commas
        {
            String s = frame.argv[1];
            int slen = s.length();
            int depth = 0;
            for(int i = 0;i < slen;i++) {
                char c = s.charAt(i);
                if(isescape(c)) {
                    result.append(c); // escape
                    c = s.charAt(++i);
                    result.append(c); // escaped char
                } else if(c == COMMA && depth == 0) {
                    result.append(semic);
                } else if(c == LPAREN) {
                    depth++;
                    result.append(c);
                } else if(c == RPAREN) {
                    depth--;
                    result.append(c);
                } else {
                    result.append(c);
                }
            }
        }
    }; //ttm_zlc

    final TTMFCN ttm_zlcp = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Zero-level commas and parentheses;
        // exact algorithm is unknown
        {
            // A(B) and A,B will both give A;B and (A),(B),C will give A;B;C
            String s = frame.argv[1];
            int slen = (s).length();
            s += EOS; // so we do not always have to check the length
            int depth = 0;
            for(int i = 0;i < slen;i++) {
                char c = s.charAt(i);
                if(isescape(c)) {
                    result.append(c);
                    i++;
                    c = s.charAt(i);
                    result.append(c);
                } else if(depth == 0 && c == COMMA) {
                    if(s.charAt(i + 1) != LPAREN) {
                        result.append(semic);
                    }
                } else if(c == LPAREN) {
                    if(depth == 0 && i > 0) {
                        result.append(semic);
                    }
                    if(depth > 0) result.append(c);
                    depth++;
                } else if(c == RPAREN) {
                    depth--;
                    if(depth == 0 && s.charAt(i + 1) == COMMA) {
                    } else if(depth == 0 && s.charAt(i + 1) == EOS) {// do nothing
                    } else if(depth == 0) {
                        result.append(semic);
                    } else {// depth > 0
                        result.append(c);
                    }
                } else {
                    result.append(c);
                }
            }
        }
    }; //ttm_zlcp

    final TTMFCN ttm_flip = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Flip a string
        {
            String s = frame.argv[1];
            int slen = s.length();
            for(int i = slen - 1;i >= 0;i--)
                result.append(s.charAt(i));
        }
    }; //ttm_flip

    final TTMFCN ttm_ccl = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Call class
        {
            Charclass cl = charclasses.get(frame.argv[1]);
            Name str = dictionary.get(frame.argv[2]);

            if(cl == null || str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            // Starting at str.residual, locate first char not in class
            int startp = str.residual;
            int endp = str.body.length();
            int p = startp;
            for(;p < endp;p++) {
                char c = str.body.charAt(p);
                if(cl.negative && (cl.characters.indexOf(c) >= 0)) break;  // not in class
                else if(!cl.negative && (cl.characters.indexOf(c) < 0)) break; // not in class
            }
            int len = (p - startp);
            if(len > 0) {
                result.append(str.body.substring(startp, p));
                str.residual += len;
            }
        }
    }; //ttm_ccl

    // Shared helper for dcl and dncl
    void
    ttm_dcl0(Frame frame, boolean negative)
    {
        Charclass cl = charclasses.get(frame.argv[1]);
        if(cl == null) {
            // create a new charclass object
            cl = new Charclass();
            cl.name = (frame.argv[1]);
            charclasses.put(cl.name, cl);
        }
        cl.characters = (frame.argv[2]);
        cl.negative = negative;
    }

    final TTMFCN ttm_dcl = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Define a class
        {
            ttm_dcl0(frame, false);
        }
    }; //ttm_dcl

    final TTMFCN ttm_dncl = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Define a negative class
        {
            ttm_dcl0(frame, true);
        }
    }; //ttm_dncl

    final TTMFCN ttm_ecl = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Erase a class
        {
            for(int i = 1;i < frame.argc;i++) {
                String clname = frame.argv[i];
                charclasses.remove(clname);
            }
        }
    }; //ttm_ecl

    final TTMFCN ttm_scl = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Skip class
        {
            Charclass cl = charclasses.get(frame.argv[1]);
            Name str = dictionary.get(frame.argv[2]);
            if(cl == null || str == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            // Starting at str.residual, locate first char not in class
            int startp = str.residual;
            int endp = str.body.length();
            int p = startp;
            for(;p < endp;p++) {
                char c = str.body.charAt(p);
                if(cl.negative && (cl.characters.indexOf(c) >= 0)) break; // not in class
                else if(!cl.negative && (cl.characters.indexOf(c) < 0)) break; // not in class
            }
            int len = (p - startp);
            str.residual += len;
        }
    }; //ttm_scl

    final TTMFCN ttm_tcl = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Test class
        {
            Charclass cl = charclasses.get(frame.argv[1]);
            Name str = dictionary.get(frame.argv[2]);
            if(cl == null)
                fail(ERR.ENONAME);
            if(str.builtin)
                fail(ERR.ENOPRIM);

            String t = frame.argv[3];
            String f = frame.argv[4];
            String retval = null;
            if(str == null)
                retval = f;
            else {
                // see if char at str.residual is in class
                char c = EOS;
                if(str.residual < str.body.length())
                    c = str.body.charAt(str.residual);
                if(cl.negative && (cl.characters.indexOf(c) < 0))
                    retval = t;
                else if(!cl.negative && (cl.characters.indexOf(c) >= 0))
                    retval = t;
                else
                    retval = f;
            }
            result.append(retval);
        }
    }; //ttm_tcl

// Arithmetic Operators

    final TTMFCN ttm_abs = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Obtain absolute value
        {
            String slhs = frame.argv[1];
            long lhs = toInt64(slhs);
            if(lhs < 0) lhs = -lhs;
            result.append(String.format("%d", lhs));
        }
    }; //ttm_abs

    final TTMFCN ttm_ad = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Add
        {
            long total = 0;
            for(int i = 1;i < frame.argc;i++) {
                String snum = frame.argv[i];
                long num = toInt64(snum);
                total += num;
            }
            result.append(String.format("%d", total));
        }
    }; //ttm_ad

    final TTMFCN ttm_dv = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Divide and give quotient
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            long lhs = toInt64(slhs);
            long rhs = toInt64(srhs);
            lhs = (lhs / rhs);
            result.append(String.format("%d", lhs));
        }
    }; //ttm_dv

    final TTMFCN ttm_dvr = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Divide and give remainder
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            long lhs = toInt64(slhs);
            long rhs = toInt64(srhs);
            lhs = (lhs % rhs);
            result.append(String.format("%d", lhs));
        }
    }; //ttm_dvr

    final TTMFCN ttm_mu = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Multiply
        {
            long total = 1;
            for(int i = 1;i < frame.argc;i++) {
                String snum = frame.argv[i];
                long num = toInt64(snum);
                total *= num;
            }
            result.append(String.format("%d", total));
        }
    }; //ttm_mu

    final TTMFCN ttm_su = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Substract
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            long lhs = toInt64(slhs);
            long rhs = toInt64(srhs);
            lhs = (lhs - rhs);
            result.append(String.format("%d", lhs));
        }
    }; //ttm_su

    final TTMFCN ttm_eq = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Compare numeric equal
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            String t = frame.argv[3];
            String f = frame.argv[4];
            long lhs = toInt64(slhs);
            long rhs = toInt64(srhs);
            result.append(lhs == rhs ? t : f);
        }
    }; //ttm_eq

    final TTMFCN ttm_gt = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Compare numeric greater-than
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            String t = frame.argv[3];
            String f = frame.argv[4];
            long lhs = toInt64(slhs);
            long rhs = toInt64(srhs);
            result.append(lhs > rhs ? t : f);
        }
    }; //ttm_gt

    final TTMFCN ttm_lt = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Compare numeric less-than
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            String t = frame.argv[3];
            String f = frame.argv[4];
            long lhs = toInt64(slhs);
            long rhs = toInt64(srhs);
            result.append(lhs < rhs ? t : f);
        }
    }; //ttm_lt

    final TTMFCN ttm_eql = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // ? Compare logical equal
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            String t = frame.argv[3];
            String f = frame.argv[4];
            result.append(slhs.compareTo(srhs) == 0 ? t : f);
        }
    }; //ttm_eql

    final TTMFCN ttm_gtl = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // ? Compare logical greater-than
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            String t = frame.argv[3];
            String f = frame.argv[4];
            result.append(slhs.compareTo(srhs) > 0 ? t : f);
        }
    }; //ttm_gtl

    final TTMFCN ttm_ltl = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // ? Compare logical less-than
        {
            String slhs = frame.argv[1];
            String srhs = frame.argv[2];
            String t = frame.argv[3];
            String f = frame.argv[4];
            result.append(slhs.compareTo(srhs) < 0 ? t : f);
        }
    }; //ttm_ltl

// Peripheral Input/Output Operations

    final TTMFCN ttm_ps = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Print a Name
        {
            String s = frame.argv[1];
            String stdxx = (frame.argc == 2 ? null : frame.argv[2]);
            PrintWriter target = null;
            if("stderr".equalsIgnoreCase(stdxx))
                target = stderr;
            else
                target = stdout;
            printstring(target, s);
        }
    }; //ttm_ps

    final TTMFCN ttm_rs = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Read a Name
        {
            for(;;) {
                int c = fgetc(rsinput);
                if(c == EOF) break;
                if(c == metac) break;
                result.append((char) c);
            }
        }
    }; //ttm_rs

    final TTMFCN ttm_psr = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Print Name and Read
        {
            int argc = frame.argc;
/*
    // force output to go to stdout
    if(argc > 2) frame.argc = 2;
*/
            ttm_ps.invoke(frame, result);
            ttm_rs.invoke(frame, result);
            frame.argc = argc;
        }
    }; //ttm_psr

    final TTMFCN ttm_cm = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Change meta character
        {
            String smeta = frame.argv[1];
            if((smeta).length() > 0) {
                if(smeta.charAt(0) > 127) fail(ERR.EASCII);
                metac = smeta.charAt(0);
            }
        }
    }; //ttm_cn

    final TTMFCN ttm_pf = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Flush stdout and/or stderr
        {
            String stdxx = (frame.argc == 1 ? null : frame.argv[1]);
            if("stdout".equalsIgnoreCase(stdxx))
                stdout.flush();
            else if("stderr".equalsIgnoreCase(stdxx))
                stderr.flush();
        }
    }; //ttm_pf

// Library Operations

    final TTMFCN ttm_names = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Obtain all Name instance names in sorted order
        {
            boolean allnames = (frame.argc > 1 ? true : false);

            // First, figure out the number of names and the total size
            int len = dictionary.size();
            if(len == 0)
                return;

            // Now collect all the names
            Object[] names = dictionary.keySet().toArray();
            Arrays.sort(names);
            // Return the set of names separated by commas
            boolean first = true;
            for(int i = 0;i < names.length;i++) {
                Name entry = dictionary.get((String) names[i]);
                if(allnames || !entry.builtin) {
                    if(!first) result.append(',');
                    result.append((String) names[i]);
                    first = false;
                }
            }
        }
    }; //ttm_names

    final TTMFCN ttm_exit = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Return from TTM
        {
            long exitcode = 0;
            flags |= FLAG_EXIT;
            if(frame.argc > 1) {
                exitcode = toInt64(frame.argv[1]);
                if(exitcode < 0) exitcode = -exitcode;
            }
            exitcode = (int) exitcode;
        }
    }; //ttm_exit

// Utility Operations

    final TTMFCN ttm_ndf = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Determine if a Name is Defined
        {
            Name str = dictionary.get(frame.argv[1]);
            String t = frame.argv[2];
            String f = frame.argv[3];
            result.append(str == null ? f : t);
        }
    }; //ttm_ndf

    final TTMFCN ttm_norm = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Obtain the Norm of a string
        {
            String s = frame.argv[1];
            result.append(String.format("%d", s.length()));
        }
    }; //ttm_norm

    final TTMFCN ttm_time = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Obtain time of day in 100'th second resolution
        {
            long time = System.currentTimeMillis();
            time = time / 10; // Need time in 100th second
            result.append(String.format("%d", time));
        }
    }; //ttm_time

    final TTMFCN ttm_xtime = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Obtain Execution Time
        {
            long delta = System.nanoTime() - starttime;
            delta = delta / 10000000; // Need time in 100th second
            result.append(String.format("%d", delta));
        }
    }; //ttm_xtime

    final TTMFCN ttm_ctime = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Convert ##<time> to printable string
        {
            String stod = frame.argv[1];
            long tod = toInt64(stod);
            tod = tod * 10; // need milliseconds
            Date today = new Date(tod);
            SimpleDateFormat format =  // Emulate C ctime()
                new SimpleDateFormat("EEE MMM dd HH:mm:ss yyyy");
            String date = format.format(today);
            result.append(date);
        }
    }; //ttm_ctime

    final TTMFCN ttm_tf = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Turn Trace Off
        {
            if(frame.argc > 1) {// trace off specific
                for(int i = 1;i < frame.argc;i++) {
                    Name fcn = dictionary.get(frame.argv[i]);
                    if(fcn == null) fail(ERR.ENONAME);
                    fcn.trace = false;
                }
            } else { // turn off all tracing
                for(String key : dictionary.keySet()) {
                    Name fcn = dictionary.get(key);
                    fcn.trace = false;
                }
                flags &= ~(FLAG_TRACE);
            }
        }
    }; //ttm_tf

    final TTMFCN ttm_tn = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Turn Trace On
        {
            if(frame.argc > 1) {// trace specific
                for(int i = 1;i < frame.argc;i++) {
                    Name fcn = dictionary.get(frame.argv[i]);
                    if(fcn == null) fail(ERR.ENONAME);
                    fcn.trace = true;
                }
            } else
                flags |= (FLAG_TRACE); /*trace all*/
        }
    }; //ttm_tn

// Functions new to this implementation

    /**
     * Get ith command line argument; as with C,
     * argv[0] is the invoking command (faked)
     */
    final TTMFCN ttm_argv = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result)
        {
            int index = (int) toInt64(frame.argv[1]);
            if(index < 0 || index >= argv.size())
                fail(ERR.ERANGE);
            String arg = argv.get(index);
            result.append(arg);
        }
    }; //ttm_argv

    /**
     * Get count of command line arguments; as with C,
     * argv[0] is the invoking command (faked)
     */
    final TTMFCN ttm_argc = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result)
        {
            int argc = argv.size();
            result.append(String.format("%d", argc));
        }
    }; //ttm_argc

    final TTMFCN ttm_classes = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Obtain all character class names
        {
            // First, figure out the number of classes
            int nclasses = charclasses.size();
            if(nclasses == 0)
                return;

            // Now collect all the class names
            Object[] clnames = charclasses.keySet().toArray();

            // Now sort the class names
            Arrays.sort(clnames);

            // Return the set of classes separated by commas
            for(int i = 0;i < clnames.length;i++) {
                if(i > 0) result.append(',');
                result.append((String) clnames[i]);
            }
        }
    }; //ttm_classes

    final TTMFCN ttm_lf = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Lock a function from being deleted
        {
            for(int i = 1;i < frame.argc;i++) {
                Name fcn = dictionary.get(frame.argv[i]);
                if(fcn == null) fail(ERR.ENONAME);
                fcn.locked = true;
            }
        }
    }; //ttm_lf

    final TTMFCN ttm_uf = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Un-Lock a function from being deleted
        {
            for(int i = 1;i < frame.argc;i++) {
                Name fcn = dictionary.get(frame.argv[i]);
                if(fcn == null) fail(ERR.ENONAME);
                fcn.locked = false;
            }
        }
    }; //ttm_uf

    final TTMFCN ttm_include = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result)  // Include text of a file
        {
            String filename = frame.argv[1];
            if(filename.length() == 0)
                fail(ERR.EINCLUDE);
            File fi = new File(filename);
            if(!fi.canRead())
                fail(ERR.EINCLUDE);
            try {
                String text = readfile(filename);
                result.append(text);
            } catch (IOException ioe) {
                fail(ERR.EIO);
            }
        }
    }; //ttm_include

/**
 Helper functions for all the ttm commands
 and subcommands
 */

    /**
     * #<ttm;meta;newmetachars>
     */
    void
    ttm_ttm_meta(Frame frame, StringBuilder result)
    {
        String arg = frame.argv[2];
        if(arg.length() != 5) fail(ERR.ETTMCMD);
        sharpc = arg.charAt(0);
        openc = arg.charAt(1);
        semic = arg.charAt(2);
        closec = arg.charAt(3);
        escapec = arg.charAt(4);
    }

    /**
     * #<ttm;info;name;...>
     */
    void
    ttm_ttm_info_name(Frame frame, StringBuilder result)
    {
        for(int i = 3;i < frame.argc;i++) {
            Name str = dictionary.get(frame.argv[i]);
            if(str == null) { // not defined
                result.append(frame.argv[i]);
                result.append("-,-,-\n");
                continue;
            }
            int namelen = str.name.length();
            result.append(str.name);
            if(str.builtin) {
                result.append(String.format(",%d", str.minargs));
                if(str.maxargs == MAXARGS)
                    result.append('*');
                else
                    result.append(String.format(",%d", str.maxargs));
                result.append(',');
                result.append(str.sideeffect ? 'S' : 'V');
            } else {
                result.append(String.format(",0,%d,V", str.maxsegmark));
            }
            if(!str.builtin) {
                result.append(String.format(" residual=%d body=|", str.residual));
                // Walk the body checking for segment and creation marks
                for(int p = 0;p < str.body.length();p++) {
                    char c = str.body.charAt(p);
                    if(ismark(c)) {
                        if(iscreate(c))
                            result.append("^00");
                        else {// segmark
                            int mark = ((int) c) & 0XFF;
                            result.append(String.format("^%02d", mark));
                        }
                    } else
                        result.append(c);
                }
                result.append('|');
            }
            result.append('\n');
        }
        if(DEBUG) {
            stderr.print("info.name: ");
            dbgprint(result.toString(), '"');
        }
    }

    /**
     * #<ttm;info;class;...>
     */
    void
    ttm_ttm_info_class(Frame frame, StringBuilder result) // Misc. combined actions
    {
        for(int i = 3;i < frame.argc;i++) {
            Charclass cl = charclasses.get(frame.argv[i]);
            if(cl == null) fail(ERR.ENONAME);
            int len = cl.name.length();
            result.append(cl.name + ' ' + LBRACKET);
            if(cl.negative) result.append('^');
            for(int p = 0;i < cl.characters.length();p++) {
                char c = cl.characters.charAt(p);
                if(c == LBRACKET || c == RBRACKET)
                    result.append('\\');
                result.append(c);
            }
            result.append('\n');
        }
        if(DEBUG) {
            stderr.printf("info.class: ");
            dbgprint(result.toString(), '"');
            stderr.printf("\n");
            stderr.flush();
        }
    }

    /**
     * #<ttm;discrim;...>
     */

    final TTMFCN ttm_ttm = new TTMFCN()
    {
        public void
        invoke(Frame frame, StringBuilder result) // Misc. combined actions
        {
            // Get the cmd string
            String cmd = frame.argv[1];

            if(frame.argc >= 3 && "meta".equalsIgnoreCase(cmd))
                ttm_ttm_meta(frame, result);
            else if(frame.argc >= 4 && "info".equalsIgnoreCase(cmd)) {
                String subcmd = frame.argv[2];
                if("name".equalsIgnoreCase(subcmd))
                    ttm_ttm_info_name(frame, result);
                else if("class".equalsIgnoreCase(subcmd))
                    ttm_ttm_info_class(frame, result);
                else
                    fail(ERR.ETTMCMD);
            } else {
                fail(ERR.ETTMCMD);
            }

        }
    }; //ttm_ttm

//////////////////////////////////////////////////

    /**
     * Builtin function table
     */

    static class Builtin
    {
        String name;
        int minargs;
        int maxargs;
        String sv;
        TTMFCN fcn;

        public Builtin(
            String name,
            int minargs,
            int maxargs,
            String sv,
            TTMFCN fcn)
        {
            this.name = name;
            this.minargs = minargs;
            this.maxargs = maxargs;
            this.sv = sv;
            this.fcn = fcn;
        }
    }

// Define a subset of the original TTM functions

    Builtin[] builtin_orig = new Builtin[]{
        // Dictionary Operations
        new Builtin("ap", 2, 2, "S", ttm_ap), // Append to a string
        new Builtin("cf", 2, 2, "S", ttm_cf), // Copy a function
        new Builtin("cr", 2, 2, "S", ttm_cr), // Mark for creation
        new Builtin("ds", 2, 2, "S", ttm_ds), // Define string
        new Builtin("es", 1, ARB, "S", ttm_es), // Erase string
        new Builtin("sc", 2, 63, "SV", ttm_sc), // Segment and count
        new Builtin("ss", 2, 2, "S", ttm_ss), // Segment a string
        // Name Selection
        new Builtin("cc", 1, 1, "SV", ttm_cc), // Call one character
        new Builtin("cn", 2, 2, "SV", ttm_cn), // Call n characters
        new Builtin("sn", 2, 2, "S", ttm_sn), // Skip n characters(Batch)
        new Builtin("cp", 1, 1, "SV", ttm_cp), // Call parameter
        new Builtin("cs", 1, 1, "SV", ttm_cs), // Call segment
        new Builtin("isc", 4, 4, "SV", ttm_isc), // Initial character scan
        new Builtin("rrp", 1, 1, "S", ttm_rrp), // Reset residual pointer
        new Builtin("scn", 3, 3, "SV", ttm_scn), // Character scan
        // Name Scanning Operations
        new Builtin("gn", 2, 2, "V", ttm_gn), // Give n characters
        new Builtin("zlc", 1, 1, "V", ttm_zlc), // Zero-level commas
        new Builtin("zlcp", 1, 1, "V", ttm_zlcp), // Zero-level commas and parentheses
        new Builtin("flip", 1, 1, "V", ttm_flip), // Flip a string(Batch)
        // Character Class Operations
        new Builtin("ccl", 2, 2, "SV", ttm_ccl), // Call class
        new Builtin("dcl", 2, 2, "S", ttm_dcl), // Define a class
        new Builtin("dncl", 2, 2, "S", ttm_dncl), // Define a negative class
        new Builtin("ecl", 1, ARB, "S", ttm_ecl), // Erase a class
        new Builtin("scl", 2, 2, "S", ttm_scl), // Skip class
        new Builtin("tcl", 4, 4, "V", ttm_tcl), // Test class
        // Arithmetic Operations
        new Builtin("abs", 1, 1, "V", ttm_abs), // Obtain absolute value
        new Builtin("ad", 2, ARB, "V", ttm_ad), // Add
        new Builtin("dv", 2, 2, "V", ttm_dv), // Divide and give quotient
        new Builtin("dvr", 2, 2, "V", ttm_dvr), // Divide and give remainder
        new Builtin("mu", 2, ARB, "V", ttm_mu), // Multiply
        new Builtin("su", 2, 2, "V", ttm_su), // Substract
        // Numeric Comparisons
        new Builtin("eq", 4, 4, "V", ttm_eq), // Compare numeric equal
        new Builtin("gt", 4, 4, "V", ttm_gt), // Compare numeric greater-than
        new Builtin("lt", 4, 4, "V", ttm_lt), // Compare numeric less-than
        // Logical Comparisons
        new Builtin("eq?", 4, 4, "V", ttm_eql), // ? Compare logical equal
        new Builtin("gt?", 4, 4, "V", ttm_gtl), // ? Compare logical greater-than
        new Builtin("lt?", 4, 4, "V", ttm_ltl), // ? Compare logical less-than
        // Peripheral Input/Output Operations
        new Builtin("cm", 1, 1, "S", ttm_cm), /*Change Meta Character*/
        new Builtin("ps", 1, 2, "S", ttm_ps), // Print a Name
        new Builtin("psr", 1, 1, "SV", ttm_psr), // Print Name and Read
/*#ifdef IMPLEMENTED
    new Builtin("rcd",2,2,"S",ttm_rcd), // Set to Read Prom Cards
#endif*/
        new Builtin("rs", 0, 0, "V", ttm_rs), // Read a Name
    /*Formated Output Operations*/
/*#ifdef IMPLEMENTED
    new Builtin("fm",1,ARB,"S",ttm_fm), // Format a Line or Card
    new Builtin("tabs",1,8,"S",ttm_tabs), // Declare Tab Positions
    new Builtin("scc",2,2,"S",ttm_scc), // Set Continuation Convention
    new Builtin("icc",1,1,"S",ttm_icc), // Insert a Control Character
    new Builtin("outb",0,3,"S",ttm_outb), // Output the Buffer
#endif*/
        // Library Operations
/*#ifdef IMPLEMENTED
    new Builtin("store",2,2,"S",ttm_store), // Store a Program
    new Builtin("delete",1,1,"S",ttm_delete), // Delete a Program
    new Builtin("copy",1,1,"S",ttm_copy), // Copy a Program
    new Builtin("show",0,1,"S",ttm_show), // Show Program Names
    new Builtin("libs",2,2,"S",ttm_libs), // Declare standard qualifiers (Batch)
#endif*/
        new Builtin("names", 0, 1, "V", ttm_names), // Obtain Name Names
        // Utility Operations
/*#ifdef IMPLEMENTED
    new Builtin("break",0,1,"S",ttm_break), // Program Break
#endif*/
        new Builtin("exit", 0, 0, "S", ttm_exit), // Return from TTM
        new Builtin("ndf", 3, 3, "V", ttm_ndf), // Determine if a Name is Defined
        new Builtin("norm", 1, 1, "V", ttm_norm), // Obtain the Norm of a Name
        new Builtin("time", 0, 0, "V", ttm_time), // Obtain time of day (modified)
        new Builtin("xtime", 0, 0, "V", ttm_xtime), // Obtain execution time(Batch)
        new Builtin("tf", 0, 0, "S", ttm_tf), // Turn Trace Off
        new Builtin("tn", 0, 0, "S", ttm_tn), // Turn Trace On
        new Builtin("eos", 3, 3, "V", ttm_eos), // Test for end of string(Batch)

/*#ifdef IMPLEMENTED
    // Batch Functions
    new Builtin("insw",2,2,"S",ttm_insw), // Control output of input monitor(Batch)
    new Builtin("ttmsw",2,2,"S",ttm_ttmsw), // Control handling of ttm programs(Batch)
    new Builtin("cd",0,0,"V",ttm_cd), // Input one card(Batch)
    new Builtin("cdsw",2,2,"S",ttm_cdsw), // Control cd input(Batch)
    new Builtin("for",0,0,"V",ttm_for), // Input next complete fortran statement(Batch)
    new Builtin("forsw",2,2,"S",ttm_forsw), // Control for input(Batch)
    new Builtin("pk",0,0,"V",ttm_pk), // Look ahead one card(Batch)
    new Builtin("pksw",2,2,"S",ttm_pksw), // Control pk input(Batch)
    new Builtin("ps",1,1,"S",ttm_ps), // Print a string(Batch)
    new Builtin("page",1,1,"S",ttm_page), // Specify page length(Batch)
    new Builtin("sp",1,1,"S",ttm_sp), // Space before printing(Batch)
    new Builtin("fm",0,ARB,"S",ttm_fm), // Format a line or card(Batch)
    new Builtin("tabs",1,10,"S",ttm_tabs), // Declare tab positions(Batch)
    new Builtin("scc",3,3,"S",ttm_scc), // Set continuation convention(Batch)
    new Builtin("fmsw",2,2,"S",ttm_fmsw), // Control fm output(Batch)
    new Builtin("time",0,0,"V",ttm_time), // Obtain time of day(Batch)
    new Builtin("des",1,1,"S",ttm_des), // Define error string(Batch)
#endif*/
    };

    // Functions new to this implementation
    Builtin[] builtin_new = new Builtin[]{
        new Builtin("argv", 1, 1, "V", ttm_argv), // Get ith command line argument
        new Builtin("argc", 0, 0, "V", ttm_argc), // Get no. of command line arguments
        new Builtin("classes", 0, 0, "V", ttm_classes), // Obtain character class Names
        new Builtin("ctime", 1, 1, "V", ttm_ctime), // Convert time to printable string
        new Builtin("include", 1, 1, "S", ttm_include), // Include text of a file
        new Builtin("lf", 0, ARB, "S", ttm_lf), // Lock functions
        new Builtin("pf", 0, 1, "S", ttm_pf), // flush stderr and/or stdout
        new Builtin("uf", 0, ARB, "S", ttm_uf), // Unlock functions
        new Builtin("ttm", 1, ARB, "SV", ttm_ttm), // Misc. combined actions
    };

    void
    defineBuiltinFunction(Builtin bin)
    {
        // Make sure we did not define builtin twice
        Name function = dictionary.get(bin.name);
        assert (function == null);
        // create a new function object
        function = new Name();
        function.builtin = true;
        function.locked = true;
        function.minargs = bin.minargs;
        function.maxargs = bin.maxargs;
        if("S".equals(bin.sv))
            function.sideeffect = true;
        function.fcn = bin.fcn;
        // Convert name to char
        function.name = bin.name;
        dictionary.put(bin.name, function);
    }

    void
    defineBuiltinFunctions()
    {
        for(Builtin bin : builtin_orig)
            defineBuiltinFunction(bin);
        for(Builtin bin : builtin_new)
            defineBuiltinFunction(bin);
    }

//////////////////////////////////////////////////

    /**
     * Startup commands: execute before
     * any other commands.
     * Beware that only the defaults instance variables are defined.
     */

    static String[] startup_commands = {
        "#<ds;comment;>",
        "#<ds;def;<##<ds;name;<text>>##<ss;name;subs>>>#<ss;def;name;subs;text>"
    };

    /**
     * Execute the commands in the set of startup_commands.
     */
    void
    startupcommands()
    {
        int saveflags = flags;
        flags &= ~FLAG_TRACE;

        for(String cmd : startup_commands) {
            scan(cmd);
        }
        flags = saveflags;
    }

//////////////////////////////////////////////////

    /**
     * Lock all functions that are currently in the dictionary
     */
    void
    lockup()
    {
        for(String key : dictionary.keySet()) {
            Name name = dictionary.get(key);
            name.locked = true;
        }
    }

//////////////////////////////////////////////////

    // Error reporting
    static class Fatal extends RuntimeException
    {
        public Fatal()
        {
            super();
        }

        public Fatal(String msg)
        {
            super(msg);
        }
    }

    void
    fail(TTM.ERR eno)
        throws Fatal
    {
        String msg = String.format("(%d) %s", eno.ordinal(), errstring(eno));
        fail(msg);
    }

    void
    fail(String msg)
        throws Fatal
    {
        System.out.flush();
        stderr.printf("Fatal error: %s\n", msg);
        if(this.stack != null) {
            // Dump the frame stack
            dumpstack();
            stderr.printf("\n");
        }
        stderr.flush();
        throw new Fatal(msg);
    }

    static String
    errstring(ERR err)
    {
        String msg = null;
        switch (err) {
        case ENOERR:
            msg = "No error";
            break;
        case ENONAME:
            msg = "Dictionary Name or Character Class Name Not Found";
            break;
        case ENOPRIM:
            msg = "Primitives Not Allowed";
            break;
        case EFEWPARMS:
            msg = "Too Few Parameters Given";
            break;
        case EFORMAT:
            msg = "Incorrect Format";
            break;
        case EQUOTIENT:
            msg = "Quotient Is Too Large";
            break;
        case EDECIMAL:
            msg = "Decimal Integer Required";
            break;
        case EMANYDIGITS:
            msg = "Too Many Digits";
            break;
        case EMANYSEGMARKS:
            msg = "Too Many Segment Marks";
            break;
        case EMEMORY:
            msg = "Dynamic Storage Overflow";
            break;
        case EPARMROLL:
            msg = "Parm Roll Overflow";
            break;
        case EINPUTROLL:
            msg = "Input Roll Overflow";
            break;
/*#ifdef IMPLEMENTED
    case EDUPLIBNAME: msg="Name Already On Library"; break;
    case ELIBNAME: msg="Name Not On Library"; break;
    case ELIBSPACE: msg="No Space On Library"; break;
    case EINITIALS: msg="Initials Not Allowed"; break;
    case EATTACH: msg="Could Not Attach"; break;
#endif*/
        case EIO:
            msg = "An I/O Error Occurred";
            break;
/*#ifdef IMPLEMENTED
    case ETTM: msg="A TTM Processing Error Occurred"; break;
    case ESTORAGE: msg="Error In Storage Format"; break;
#endif*/
        case ENOTNEGATIVE:
            msg = "Only unsigned decimal integers";
            break;
        // messages new to this implementation
        case ESTACKOVERFLOW:
            msg = "Stack overflow";
            break;
        case ESTACKUNDERFLOW:
            msg = "Stack Underflow";
            break;
        case EBUFFERSIZE:
            msg = "Buffer overflow";
            break;
        case EMANYINCLUDES:
            msg = "Too many includes";
            break;
        case EINCLUDE:
            msg = "Cannot read Include file";
            break;
        case ERANGE:
            msg = "index out of legal range";
            break;
        case EMANYPARMS:
            msg = "Number of parameters greater than MAXARGS";
            break;
        case EEOS:
            msg = "Unexpected end of string";
            break;
        case EASCII:
            msg = "ASCII characters only";
            break;
        case ECHAR8:
            msg = "Illegal utf-8 character set";
            break;
        case EUTF32:
            msg = "Illegal utf-32 character set";
            break;
        case ETTMCMD:
            msg = "Illegal #<ttm> command";
            break;
        case ETIME:
            msg = "Gettimeofday() failed";
            break;
        case EEXECCOUNT:
            msg = "too many executions";
            break;
        case EOTHER:
            msg = "Unknown Error";
            break;
        }
        return msg;
    }

//////////////////////////////////////////////////
// Debug utility functions

    void
    dumpnames()
    {
        // First, figure out the number of names and the total size
        int len = dictionary.size();
        if(len == 0)
            return;

        // Now collect all the names
        Object[] names = dictionary.keySet().toArray();
        Arrays.sort(names);
        // Return the set of names one per line
        for(int i = 0;i < names.length;i++) {
            stderr.println((String) names[i]);
        }
        stderr.flush();
    }

    void
    dumpframe(Frame frame)
    {
        traceframe(frame, true);
        stderr.println("");
        stderr.flush();
    }

//////////////////////////////////////////////////
// Utility functions

    long
    toInt64(String s)
    {
        try {
            long n = Long.parseLong(s);
            return n;
        } catch (NumberFormatException nfe) {
            fail(ERR.EDECIMAL);
            return 0;
        }
    }

    /**
     * Given a char, return its escaped value.
     * Zero indicates it should be elided
     */
    char
    convertEscapeChar(char c)
    {
        // de-escape and store
        switch (c) {
        case 'r':
            c = '\r';
            break;
        case 'n':
            c = '\n';
            break;
        case 't':
            c = '\t';
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case '\n':
            c = 0;
            break; // escaped eol is elided
        default:
            break; // no change
        }
        return c;
    }

    /**
     * Print a given single frame
     *
     * @param frame       the frame to print
     * @param includeargs true if frame arguments should be printed
     */
    void
    traceframe(Frame frame, boolean includeargs)
    {
        String tag = "";
        if(frame.argc == 0) {
            stderr.print("#<empty frame>");
            stderr.flush();
            return;
        }
        tag += sharpc;
        if(!frame.active)
            tag += sharpc;
        tag += openc;
        stderr.print(tag + frame.argv[0]);
        if(includeargs) {
            for(int i = 1;i < frame.argc;i++) {
                stderr.print("" + semic);
                dbgprint(frame.argv[i], NUL);
            }
        }
        stderr.print(closec);
        stderr.flush();
    }

    /**
     * Helper function for trace(). Trace single stack frame.
     *
     * @param depth    the position in the frame stack to trace
     * @param entering true if we are tracing
     *                 the start of a function call
     * @param tracing  true if we are tracing as opposed to dumping
     */

    void
    trace1(int depth, boolean entering, boolean tracing)
    {
        if(depth < 0) fail(ERR.ESTACKUNDERFLOW);
        Frame frame = stack.get(depth);
        stderr.printf("[%02d] ", depth);
        if(tracing)
            stderr.printf("%s: ", (entering ? "begin" : "end"));
        traceframe(frame, entering);
        // Dump the contents of result if !entering
        if(!entering) {
            stderr.print(" => ");
            if(frame.result != null)
                dbgprint(frame.result.toString(), '"');
            else
                dbgprint("", '"');
        }
        stderr.println("");
        stderr.flush();
    }

    /**
     * Trace a top frame in the frame stack.
     *
     * @param entering true if we are tracing
     *                 the start of a function call
     * @param tracing  true if we are tracing as opposed to dumping
     */
    void
    trace(boolean entering, boolean tracing)
    {
        trace1(stack.size() - 1, entering, tracing);
    }

//////////////////////////////////////////////////

// Debug Support

    /**
     * Dump the stack
     */
    void
    dumpstack()
    {
        for(int i = 0;i < stack.size();i++) {
            trace1(i, false, !TRACING);
        }
        stderr.flush();
    }

    void
    dbgprintc(char c, char quote)
    {
        if(ismark(c)) {
            String info = null;
            if(iscreate(c))
                info = "^00";
            else { // segmark
                int index = ((int) c) & 0xFF;
                info = String.format("^%02d", index);
            }
            stderr.print(info);
        } else if(iscontrol(c)) {
            stderr.print('\\');
            switch (c) {
            case '\r':
                stderr.print('r');
                break;
            case '\n':
                stderr.print('n');
                break;
            case '\t':
                stderr.print('t');
                break;
            case '\b':
                stderr.print('b');
                break;
            case '\f':
                stderr.print('f');
                break;
            default: {
                // dump as a decimal character
                stderr.printf("\\%d", (int) c);
            }
            break;
            }
        } else {
            if(c == quote) stderr.print('\\');
            stderr.print(c);
        }
    }

    void
    dbgprint(StringBuilder sb, char quote)
    {
        dbgprint(sb == null ? "" : sb.toString(), quote);
    }

    void
    dbgprint(String s, char quote)
    {
        char c;
        if(quote != NUL)
            dbgprintc(quote, NUL);
        for(int i = 0;i < s.length();i++) {
            c = s.charAt(i);
            dbgprintc(c, quote);
        }
        if(quote != NUL)
            dbgprintc(quote, NUL);
        stderr.flush();
    }

//////////////////////////////////////////////////
// Interpreter IO

    /**
     * Read from input until the
     * read characters form a
     * balanced string with respect
     * to the current open/close characters.
     * Read past the final balancing character
     * to the next end of line.
     *
     * @param buffer into which the characters read are appended.
     * @return false if the read was terminated
     *         by EOF, true otherwise.
     * @throws IOException
     */

    boolean
    readbalanced(StringBuilder buffer)
    {
        // Read character by character until EOF; take escapes and open/close
        // into account; keep outer <...>
        int depth = 0;
        int c;
        while((c = fgetc(rsinput)) >= 0) {
            if(c == escapec) {
                buffer.append((char) c);
                c = fgetc(rsinput);
            }
            buffer.append((char) c);
            if(c == openc) {
                depth++;
            } else if(c == closec) {
                depth--;
                if(depth == 0) break;
            }
        }
        // skip to end of line
        while(c > 0 && c != '\n') {
            c = fgetc(rsinput);
        }
        return (c == '\n' ? true : false);
    }

// Character Input/Output

// Unless you know that you are outputing ASCII,
// all output should go thru this procedure.

    /**
     * Output a single character to file f
     *
     * @param c character to output
     * @param f the outputstream
     * @throws IOException
     */

    void
    fputc(char c, Writer f)
    {
        try {
            f.write(c);
        } catch (IOException ioe) {
            fail(ERR.EIO);
        }
    }

    /**
     * Output a string to file f
     *
     * @param s string to output
     * @param f the outputstream
     * @throws IOException
     */

    void
    fputs(String s, Writer f)
    {
        for(int i = 0;i < s.length();i++)
            fputc(s.charAt(i), f);
    }

    /**
     * Read a single character from file f
     *
     * @param f the inputstream
     * @return the character read
     * @throws IOException
     */
    int
    fgetc(Reader f)
    {
        try {
            return f.read();
        } catch (IOException ioe) {
            fail(ERR.EIO);
            return 0;
        }
    }

//////////////////////////////////////////////////
// Main() Support functions

    /**
     * Print a usage message and exit
     *
     * @param msg msg to print along with usage info
     */
    static void
    usage(final String msg)
    {
        if(msg != null)
            System.err.printf("%s\n", msg);
        System.err.printf(
            "usage: java"
                + "[-Ddebug=string]"
                + "[-Dexec=string]"
                + "[-Dprogram=file]"
                + "[-Doutput=file]"
                + "[-Dinput=file]"
                + "[-Dinteractive]"
                + "[-Dversion]"
                + "[-Dquiet]"
                + "[-DX=tag:value,...]"
                + " -jar jar [arg...]\n");
        if(msg != null) System.exit(1);
        else System.exit(0);
    }

    /**
     * Read the contents of a file assuming
     *
     * @param filename file to read, '-' => stdin.
     * @return The contents of the file as a string
     * @throws IOException
     */

    static String
    readfile(String filename)
        throws IOException
    {
        InputStream input = null;
        input = new FileInputStream(filename);
        InputStreamReader rdr = new InputStreamReader(input, UTF8);

        int c = 0;
        StringBuilder buf = new StringBuilder();
        while((c = rdr.read()) >= 0) {
            buf.append((char) c);
        }
        input.close();
        return buf.toString();
    }

    static long
    tagvalue(String p)
    {
        long value;
        int c;
        if(p == null || p.length() == 0)
            return -1;
        String[] pieces = p.split("[mMkK]");
        if(pieces.length == 0 || pieces.length > 2)
            return -1;
        String digits = pieces[0];
        long multiplier = 1;
        if(pieces.length == 2) {
            if(pieces[1].equals("m") || pieces[1].equals("M"))
                multiplier = (1 << 20);
            else if(pieces[1].equals("k") || pieces[1].equals("K"))
                multiplier = (1 << 10);
            else
                return -1;
        }
        try {
            value = Long.parseLong(digits);
        } catch (NumberFormatException nfe) {
            return -1;
        }
        return value * multiplier;
    }

//////////////////////////////////////////////////

    /**
     * Initialize and start the TTM instance
     *
     * @param argv the argv from the static main
     * @throws Fatal
     */

    static public void
    main(String[] argv)
    {
        // Get the -D flags from command line
        String execcmd = System.getProperty("exec");
        String executefilename = System.getProperty("program");
        String outputfilename = System.getProperty("output");
        String inputfilename = System.getProperty("input");
        boolean interactive = (System.getProperty("interactive") != null);
        boolean quiet = (System.getProperty("quiet") != null);

        String version = System.getProperty("version");
        if(version != null) {
            System.err.println("TTM version = " + VERSION);
            return;
        }

        // Complain if interactive and output file name specified
        if(outputfilename != null && interactive) {
            usage("Interactive is illegal if output file specified");
        }

        // Process the -Ddebug flags
        String debugflags = System.getProperty("debug");
        if(debugflags != null) { // -Ddebug=<debugflags>
            for(int i = 0;i < debugflags.length();i++) {
                char c = debugflags.charAt(i);
                switch (c) {
                case 't':
                    TTM.testing = true;
                    break;
                case 'b':
                    TTM.bare = true;
                    break;
                default:
                    usage("Illegal -Ddebug option: " + c);
                }
            }
        }


        long buffersize = 0;
        long stacksize = 0;
        long execcount = 0;
        String xoption = System.getProperty("X");
        if(xoption != null) { // -DX=<xoption>
            String[] pieces = xoption.split("[ \t]*,[ \t]*");
            for(String piece : pieces) {
                String[] tagparts = piece.split("[ \t]*:[ \t]*");
                long size = -1;
                if(tagparts.length != 2
                    || tagparts[0].length() == 0
                    || (size = tagvalue(tagparts[1])) <= 0)
                    usage("Illegal -DX option: " + piece);
                switch (tagparts[0].charAt(0)) {
                case 'b':
                    buffersize = size;
                    break;
                case 's':
                    stacksize = size;
                    break;
                case 'x':
                    execcount = size;
                    break;
                default:
                    usage("Illegal -DX option: " + piece);
                }
            }
        }

        // Create the interpreter
        TTM ttm = new TTM();

        // set any X and debug flags
        ttm.setLimits(buffersize, stacksize, execcount);

        // Collect any args for #<arg>
        ttm.addArgv(ARGV0); // pretend
        for(String arg : argv)
            ttm.addArgv(arg);

        if(outputfilename == null) {
            ttm.setOutput(ttm.getStdout(), true);
        } else {
            File f = new File(outputfilename);
            if(!f.canWrite()) {
                usage("Output file is not writable: " + outputfilename);
            }
            try {
                ttm.setOutput(
                    new PrintWriter(new OutputStreamWriter(
                        new FileOutputStream(f), UTF8), true),
                    false);
            } catch (FileNotFoundException fnf) {
                usage("File not found: " + f);
            }
        }

        if(inputfilename == null) {
            ttm.setInput(ttm.getStdin(), true);
        } else {
            File f = new File(inputfilename);
            if(!f.canRead()) {
                usage("-Dinput file is not readable: " + inputfilename);
            }
            try {
                ttm.setInput(
                    new InputStreamReader(new FileInputStream(f), UTF8),
                    false);
            } catch (FileNotFoundException fnf) {
                usage("File not found: " + f);
            }
        }

        boolean stop = false;

        // Execute the -Dexec string
        if(execcmd != null) {
            ttm.scan(execcmd);
            if((ttm.testFlag(FLAG_EXIT))) stop = true;
        }

        // Now execute the executefile, if any
        if(!stop && executefilename != null) {
            try {
                String text = readfile(executefilename);
                String result = ttm.scan(text);
                if(ttm.testFlag(FLAG_EXIT)) stop = true;
                if (!quiet)
                  ttm.printstring(ttm.stdout, result);
            } catch (IOException ioe) {
                usage("Cannot read -f file: " + ioe.getMessage());
            }
        }

        // If interactive, start read-eval loop
        if(!stop && interactive) {
            ttm.evalloop();
        }
        // cleanup
        if(!ttm.isstdout()) ttm.getOutput().close();
        try {
            if(!ttm.isstdin()) ttm.getInput().close();
        } catch (IOException ioe) {
        }

        System.exit(ttm.getExitcode());
    }

}//TTM


