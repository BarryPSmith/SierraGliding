###############################################################################
#
# vtab.tcl --
#
# Written by Joe Mistachkin.
# Released to the public domain, use at your own risk!
#
###############################################################################

proc readFile { fileName } {
  set file_id [open $fileName RDONLY]
  fconfigure $file_id -encoding binary -translation binary
  set result [read $file_id]
  close $file_id
  return $result
}

proc writeFile { fileName data } {
  set file_id [open $fileName {WRONLY CREAT TRUNC}]
  fconfigure $file_id -encoding binary -translation binary
  puts -nonewline $file_id $data
  close $file_id
  return ""
}

proc escapeSubSpec { data } {
  regsub -all -- {&} $data {\\\&} data
  regsub -all -- {\\(\d+)} $data {\\\\\1} data
  return $data
}

proc preProcessLine { value } {
  regsub -- { id="[0-9a-z_]+"} $value "" value
  regsub -- {</p><h(\d)>} $value </p>\n<h\\1> value
  regsub -- {</p><blockquote><pre>} $value <blockquote><pre> value
  return $value
}

proc englishToList { value } {
  set result [list]

  foreach element [split $value "\t\n ,"] {
    if {[string tolower $element] ni [list "" and or]} then {
      lappend result $element
    }
  }

  return $result
}

proc processLine { line prefix ltAndGt } {
  if {[string length [string trim $line]] == 0 || \
      [regexp -- {<h\d(?: |>)} [string range $line 0 3]] || \
      [regexp -- {</p>\n<h\d(?: |>)} [string range $line 0 8]]} then {
    return ""
  }

  set result $line

  foreach remove [list \
      {<a name=".*?">} {<a href=".*?">} {</a>} {<p>} {</p>}] {
    regsub -all -- $remove $result "" result

    if {[string length [string trim $result]] == 0} then {
      return ""
    }
  }

  foreach escape [list \
      {<b>} {</b>} {<br>} {</br>} {<dd>} {</dd>} {<dl>} {</dl>} \
      {<dt>} {</dt>} {<li>} {</li>} {<ol>} {</ol>} {<tt>} {</tt>} \
      {<ul>} {</ul>}] {
    regsub -all -- ($escape) $result {<![CDATA[\1]]>} result
  }

  regsub -all -- {&ne;} $result {\&#8800;} result
  regsub -all -- {&#91(?:;)?} $result {[} result
  regsub -all -- {&#93(?:;)?} $result {]} result

  if {$ltAndGt} then {
    regsub -all -- {<( |\"|\d|=)} $result {\&lt;\1} result
    regsub -all -- {( |\"|\d|=)>} $result {\1\&gt;} result
  }

  regsub -all -- { < } $result { \&lt; } result
  regsub -all -- { > } $result { \&gt; } result

  regsub -all -- {<div class="codeblock"><pre>} $result \
      <para><code>\n${prefix} result

  regsub -all -- {</pre></div>} $result </code></para> result
  regsub -all -- {<blockquote><pre>} $result <para><code> result
  regsub -all -- {</pre></blockquote>} $result </code></para> result
  regsub -all -- {<blockquote>} $result <para><code> result
  regsub -all -- {</blockquote>} $result </code></para> result

  return $result
}

proc extractMethod {
        name lines pattern prefix indexVarName methodsVarName ltAndGt } {
  upvar 1 $indexVarName index
  upvar 1 $methodsVarName methods

  array set levels {p 0}
  set length [llength $lines]

  while {$index < $length} {
    set line [preProcessLine [lindex $lines $index]]

    if {[regexp -- $pattern $line]} then {
      break; # stop on this line for outer loop.
    } else {
      set trimLine [string trim $line]; set data ""

      if {$levels(p) > 0 && [string length $trimLine] == 0} then {
        # blank line, close paragraph.
        if {[info exists methods($name)]} then {
          # non-first line, leading line separator.
          append data \n $prefix </para>
        } else {
          # first line, no leading line separator.
          append data $prefix </para>
        }

        incr levels(p) -1
      } elseif {[string range $trimLine 0 2] eq "<p>" || \
          [string range $trimLine 0 6] eq "</p><p>"} then {
        # open tag ... maybe one line?
        if {[string range $trimLine end-3 end] eq "</p>"} then {
          set newLine [processLine $line $prefix $ltAndGt]

          if {[string length $newLine] > 0} then {
            # one line tag, wrap.
            if {[info exists methods($name)]} then {
              # non-first line, leading line separator.
              append data \n $prefix <para>
            } else {
              # first line, no leading line separator.
              append data $prefix <para>
            }

            append data \n $prefix $newLine
            append data \n $prefix </para>
          }
        } else {
          if {[info exists methods($name)]} then {
            # non-first line, leading line separator.
            append data \n $prefix <para>
          } else {
            # first line, no leading line separator.
            append data $prefix <para>
          }

          set newLine [processLine $line $prefix $ltAndGt]

          if {[string length $newLine] > 0} then {
            append data \n $prefix $newLine
          }

          incr levels(p)
        }
      } else {
        set newLine [processLine $line $prefix $ltAndGt]

        if {[string length $newLine] > 0} then {
          if {[info exists methods($name)]} then {
            # non-first line, leading line separator.
            append data \n $prefix $newLine
          } else {
            # first line, no leading line separator.
            append data $prefix $newLine
          }
        }
      }

      if {[string length $data] > 0} then {
        append methods($name) $data
      }

      incr index; # consume this line for outer loop.
    }
  }
}

#
# NOTE: This is the entry point for this script.
#
set path [file normalize [file dirname [info script]]]

set coreDocPath [file join $path Special Core]
set interfacePath [file join [file dirname $path] System.Data.SQLite]
set inputFileName [file join $coreDocPath vtab.html]

if {![file exists $inputFileName]} then {
  puts "input file \"$inputFileName\" does not exist"
  exit 1
}

set outputFileName [file join $interfacePath ISQLiteNativeModule.cs]

if {![file exists $outputFileName]} then {
  puts "output file \"$outputFileName\" does not exist"
  exit 1
}

set inputData [readFile $inputFileName]

set inputData [string map [list \
    {<font size="6" color="red">*** DRAFT ***</font>} ""] $inputData]

set inputData [string map [list {<p align="center"></p>} ""] $inputData]

if {[string first &lt\; $inputData] != -1 || \
    [string first &gt\; $inputData] != -1} then {
  set ltAndGt false
} else {
  set ltAndGt true
}

set lines [split [string map [list \r\n \n] $inputData] \n]

set patterns(start) [string trim {
  ^(?:</p>\n)?<h1>(?:<span>)?2\. (?:</span>)?Virtual Table Methods</h1>$
}]

set patterns(method) [string trim {
  ^(?:</p>\n)?<h2>(?:<span>)?2\.\d+\. (?:</span>)?The (.*) Method(?:s)?</h2>$
}]

set prefix "        /// "
unset -nocomplain methods; set start false

for {set index 0} {$index < [llength $lines]} {} {
  set line [preProcessLine [lindex $lines $index]]

  if {$start} then {
    if {[regexp -- $patterns(method) $line dummy capture]} then {
      foreach method [englishToList $capture] {
        set methodIndex [expr {$index + 1}]

        extractMethod $method $lines $patterns(method) $prefix \
            methodIndex methods $ltAndGt
      }

      set index $methodIndex
    } else {
      incr index
    }
  } elseif {[regexp -- $patterns(start) $line]} then {
    set start true; incr index
  } else {
    incr index
  }
}

set outputData [string map [list \r\n \n] [readFile $outputFileName]]
set count 0; set start 0

#
# NOTE: These method names must be processed in the EXACT order that they
#       appear in the output file.
#
foreach name [list \
    xCreate xConnect xBestIndex xDisconnect xDestroy xOpen xClose \
    xFilter xNext xEof xColumn xRowid xUpdate xBegin xSync xCommit \
    xRollback xFindFunction xRename xSavepoint xRelease xRollbackTo] {
  #
  # HACK: This assumes that a line of 71 forward slashes will be present
  #       before each method, except for the first one.
  #
  if {$count > 0} then {
    set start [string first [string repeat / 71] $outputData $start]
  }

  set pattern ""

  append pattern ^ {\s{8}} "/// <summary>"
  append pattern {((?:.|\n)*?)}
  append pattern {\n\s{8}} "/// </summary>"
  append pattern {(?:(?:.|\n)*?)}
  append pattern {\n\s{8}[\w]+?\s+?} $name {\($}

  if {[regexp -nocase -start \
      $start -line -indices -- $pattern $outputData dummy indexes]} then {
    set summaryStart [lindex $indexes 0]
    set summaryEnd [lindex $indexes 1]

    set outputData [string range \
        $outputData 0 $summaryStart]$methods($name)[string \
        range $outputData [expr {$summaryEnd + 1}] end]

    incr count; incr start [expr {[string length $methods($name)] + 1}]
  } else {
    error "cannot find virtual table method \"$name\" in \"$outputFileName\""
  }
}

if {$count > 0} then {
  writeFile $outputFileName [string map [list \n \r\n] $outputData]
}

exit 0
