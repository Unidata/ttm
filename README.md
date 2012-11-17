# Project Description
The goal of the **ttm** project is to provide
a reference implementation for the TTM programming language.

TTM is a form of macro processor similar to TRAC, GAP, and GPM. 

# Syntax and Semantics
It is assumed that TTM is given a text file containing some combination
of ordinary text and TTM function calls (i.e. invocations).
The text is scanned character by character. Any ordinary text is passed
to the output unchanged (except when escaped).
If a TTM function is encountered, it is collected and executed.
The general form of a TTM function call looks like this.
<pre>
#&lt;functionname;arg1;arg2;...;argn&gt;
</pre>
where the functionname and the arguments are arbitrary character strings
not characters of significance: '#', '&lt;', '&gt;', and ';'.
The function is invoked with the specified arguments and the resulting
text is inserted into the original text in place of the function call.
If the function call was prefixed by a single '#' character, then scanning
will resume just ''before'' the inserted text from the function call. If the
function call was prefixed by two '#' characters, then scanning
resumes just ''after'' the inserted text.

During the collection of a function call, additional function calls
may be encountered, for example, this.
<pre>
#&lt;functionname;arg1;#&lt;f2;arg;...&gt;;...;argn&gt;
</pre>
The nested function call will be invoked when encountered and the result
inserted into the text of the outer function call and scanning of
the outer function call resumes at the place indicated by the number
of '#' characters preceding the nested call.

If a function takes, for example, 2 arguments, any extras
are ignored. For user defined functions, if too few arguments
are provided, additional one are added with the value of the empty
string ("").

As with other
applicative programming languages,
a TTM function may be recursive and may be defined as the result
of the invocation of a sequence of other function calls.

Functions are either ''built-in'' or user defined. A large number of built-in
functions exist and are defined in the TTM reference manual.

https://github.com/Unidata/ttm/blob/master/ttm_batch_processing_pr_08.pdf
# External Links
* [https://github.com/Unidata/ttm] A reference implementation of TTM] on [[github]].
* [https://github.com/Unidata/ttm/blob/master/ttm_interpretive_language_pr_07.pdf|Caine, S.H. and Gordon, E.K., TTM: An Experimental Interpretive Language. California Institute of Technology, Willis H. Booth Computing Center, Programming Report No. 7, 1968.]
* [https://github.com/Unidata/ttm/blob/master/ttm_batch_processing_pr_08.pdf|Caine, S.H. and Gordon, E.K., TTM: A Macro Language for Batch Processing. California Institute of Technology, Willis H. Booth Computing Center, Programming Report No. 8 May, 1969.]
