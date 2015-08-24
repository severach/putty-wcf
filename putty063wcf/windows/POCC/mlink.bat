REM Pelles 8.0 no longer allows .. in project file names
REM Make a Windows link that allows the project file to stay here
REM This is a recursive link so you may need to get rid of it.
REM Use RMDIR to remove this link. DO NOT USE DEL.
REM http://superuser.com/questions/167076/how-can-i-delete-a-symbolic-link

REM Dir Links are no good. Explorer follows them.
REM mklink /d src ..\..\

mklink /j src ..\..\
