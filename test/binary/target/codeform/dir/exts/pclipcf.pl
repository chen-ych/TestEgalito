# pclipcf.pl by DWK
# Perl script to run codeform with the clipboard for input and output
#
# Usage: clipcf.pl [-s start-seq | -p program] [path-to-codeform [arguments]]
#
# If a condition is met, executes codeform on the clipboard's contents and
# puts the output back into the clipboard. Otherwise, does nothing.
#
# If the clipboard starts with the start sequence, the condition is met. (The
# default condition is a start sequence of "[cf]".) Alternatively (or
# additionally), the condition can be the return value of a program: true if
# the condition was met. (An example of this is shiftdown.exe, which returns
# true if the shift key is held down.)
#
# This script is Windows-only and not thread-safe!
# In other words, don't run multiple instances of it.
# It takes no CPU except when the clipboard is changed (on the OSes tested).

use Win32::Clipboard;

my $clip = Win32::Clipboard();

my $start = undef;  # Start sequence (set to "[cf]" by default)
my $program = undef;  # Program to execute and check return value of
my $args = 'rules\cpp_1_vbb';  # Default arguments to pass to codeform
my $tmp = './pclipcf.tmp';  # Temporary output file

if(defined(@ARGV)) {
    &parse_arguments();
}

&wait_clip();

# Parses the command-line arguments passed to the script.
sub parse_arguments {
    my $x = 0;
    
    if($ARGV[$x] eq '-s') {
        $x ++;
        $start = $ARGV[$x++];
    }
    elsif($ARGV[$x] eq '-p') {
        $x ++;
        $program = $ARGV[$x++];
    }
    else {
        $start = '[cf]';  # Default start sequence
    }
    
    if(defined($ARGV[$x])) {
        chdir($ARGV[$x++]) or die "Invalid directory\n";
    }
    if(defined($ARGV[$x])) {
        $args = $ARGV[$x];
    }
}

# Waits indefinitely until the clipboard is modified, then checks to see if
# the clipboard starts with the start sequence, or the program to be executed
# returns true. If either does, calls &codeform().
sub wait_clip {
    my $data;
    
    for(;;) {
        $clip->WaitForChange();
        $data = $clip->GetText();
        
        if(defined($start) && $start eq substr($data, 0, length($start))) {
            &codeform(substr($data, length($start)));
        }
        elsif(defined($program) && system($program) >> 8) {
            &codeform($data);
        }
    }
}

# Clears the clipboard; executes codeform, supplying it with the data in $_[0]; then calls
# &set_clip().
sub codeform {
    $clip->Empty();
    
    # Strip trailing spaces and the newline after the start sequence
    $_[0] =~ s/^\s+\n//;
    
    open(CODEFORM, "|codeform -o $tmp $args");
    print CODEFORM $_[0];
    close CODEFORM;
    
    &set_clip();
}

# Sets the clipboard to the output from codeform, in the file $tmp, and
# deletes the temporary file when it is done with it.
sub set_clip {
    open(OUTPUT, "<$tmp");
    my @outd = <OUTPUT>;
    close OUTPUT;
    
    unlink $tmp;
    
    $clip->Set(join('', @outd));
}
