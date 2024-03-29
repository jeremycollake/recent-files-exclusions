# Recent Files Exclusions

Recent Files Exclusions provides a way to recursively exclude files and folders from the Windows Recent Files list (the lower pane in Explorer�s home view). This capability is not otherwise available since Windows only allows hiding specific files from Recent Items, after the fact.

## Why is it necessary?

Windows only provides the ability to exclude specific files, one by one, and removal of a folder from Quick Access does not prevent its files from being added to the Recent Files list.

## How does it work?

Recent Files Exclusions monitors the Recent Items database in Windows and prunes it in real-time when a change occurs. 
You can exclude any path or filenames by using a matching substring. For instance, `accounting` would match all files in `c:\users\jeremy\accounting`, or a specific filename like `accounting-export.csv`.

## How do I use it?

Simply install and it�ll live in your system tray, pruning your Recent Files in real-time. 
You can specify your exclusion list by clicking the tray icon to open its main window, or re-launching the app.

![Recent Files Exclusions screenshot](https://github.com/jeremycollake/recentitemsexclusions/blob/master/screenshots/recentfilesexclusions.png?raw=true)