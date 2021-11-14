# datesgit
A method to recover the original dates in git clone repositories

Windows Command Line application to manage original dates in files shared by git. WRitten in C. It requires cJSON lirary to be compiled.

The Git system does not relay on dates to detect changes. Dates are not stored by a git repository and are no communicated back in a pull operation.
This prevents to combine git with local comparisions based on date criteria.
This program overcomes this problem by adding a datesgit.json file to the git repository with the actual dates of the file.

* Use DatesGit STORE before creating a commit and push, to store the dates and sent them to the git repository
* Use DatesGit RESTORE after requesting a pull, to restore the dates to their original values

DatesGit STORE creates a file datesgit.json with the original creation and last write dates for each file on a folder and their subfolders
The format of the JSON file is explained in this example
```
{
        "[folder_name]":  //Optional
                "[subfolder_name]":   //Optional
                        "[subsubfolder_name]":   //Optional recursively
                                "[file_name]":
                                        "cre":{   //Creation date
                                                "Y":2021,"M":11,"D":14,"h":7,"mi":52,"s":7,"ms":670
                                        }
                                        "mod":{   //Last write date
                                                "Y":2021,"M":11,"D":14,"h":7,"mi":52,"s":7,"ms":670
                                        }
                                }
                        }
                }
        }
}
```
Use one of the following syntax:
```
DatesGit /?
DatesGit STORE [git clone folder]
DatesGit RESTORE [git clone folder]
```
