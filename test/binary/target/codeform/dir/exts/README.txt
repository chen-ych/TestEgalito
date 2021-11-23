This directory, exts/, contains several extensions to codeform. Perl is
required to run many of them. You can get Perl for Windows at:
    http://activestate.com/

codeform.pl is used to host codeform online at
http://dwks.theprogrammingsite.com/myprogs/down/codeform_online/cfonline.htm

The rest of the files are related to clipboard extensions. You can run them by
executing the batch files or directly from the command line, with your own
options.

Here are the files in exts/ and a brief description:
asyncdown.c         C source code for asyncdown
asyncdown.exe       Returns true if a given key is held down
clip_inout_any.bat  Codeforms the data from and to the clipboard
clip_inout_vbb.bat  The same, but rules\cpp_1_vbb is added to the args
clip_shift.bat*     Codeforms the clipboard when CTRL-SHIFT-C is pressed
clip_start.bat*     Codeforms the clipboard when it starts with "[cf]"
clip_start_any.bat* The same, but when it starts with the first argument
clipclr.c           C source code for clipclr
clipclr.exe         Clears the clipboard
clipget.c           C source code for clipget
clipget.exe         Prints the contents of the clipboard (like pclipget.pl)
clipset.c           C source code for clipset
clipset.exe         Stores its input into the clipboard (like pclipset.pl)
cliptee.c           C source code for cliptee
cliptee.exe         Stores input into the clipboard, but also prints the data
codeform.pl*        Perl script for codeform online
pclipcf.pl*         Codeforms the clipboard for multiple conditions
pclipcf4.pl*        (old) Codeforms the clipboard when it starts with a string
pclipclr.pl*        Clears the clipboard
pclipget.pl*        Prints the contents of the clipboard to the screen
pclipset.pl*        Sets the clipboard to the data it is passed (from stdin)
pcliptee.pl*        Stores input into the clipboard, but also prints the data
presdir.exe         CDs somewhere, executes a command, then CDs back
presdir.c           C source code for presdir
README.txt          This readme file
shiftdown.c         C source code for shiftdown
shiftdown.exe       Returns true if either shift key is currently held down

Programs marked with an asterisk (*) require Perl to run.
