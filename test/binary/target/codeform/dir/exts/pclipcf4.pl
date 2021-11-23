# pclipcf4.pl by DWK
# Perl script to run codeform with the clipboard for input and output
#
# Usage: pclipcf4.pl [-s start-sequence] [path-to-codeform [arguments]]
#
# If the clipboard starts with the start sequence, executes codeform on the
# clipboard's contents and puts the output back into the clipboard.
# Otherwise, does nothing.
#
# The start sequence is by default "[cf]".
#
# This script is Windows-only and not thread-safe!
# In other words, don't run multiple instances of it.
# It takes no CPU except when the clipboard is changed (on the OSes tested).

use Win32::Clipboard;

my $clip = Win32::Clipboard();

my $start = '[cf]';  # Default start sequence
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
        if(defined($ARGV[++$x])) {
            $start = $ARGV[$x++];
        }
    }
    
    if(defined($ARGV[$x])) {
        chdir($ARGV[$x++]) or die "Invalid directory\n";
    }
    if(defined($ARGV[$x])) {
        $args = $ARGV[$x];
    }
}

# Waits indefinitely until the clipboard is modified, then checks to see if
# the clipboard starts with the start sequence. If it does, calls &codeform().
sub wait_clip {
    my $data;
    
    for(;;) {
        $clip->WaitForChange();
        $data = $clip->GetText();
        
        if($start eq substr($data, 0, length($start))) {
            $clip->Empty();
            &codeform(substr($data, length($start)));
            &set_clip();
        }
    }
}

# Executes codeform, supplying it with the data in $_[0].
sub codeform {
    # Strip trailing spaces and the newline after the start sequence
    $_[0] =~ s/^\s+\n//;

    open(CODEFORM, "|codeform -o $tmp $args");
    print CODEFORM $_[0];
    close CODEFORM;
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
