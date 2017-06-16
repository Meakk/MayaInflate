Add a new command in Maya to inflate or deflate the selected closed mesh.
It changes the volume of the mesh while minimizing the distortion.

# MayaInflate

Create a new function `inflate` which inflate or deflate the volume of selected mesh.
The mesh must be watertight.

Use : `inflate -ratio 2 -iterations 15`

`ratio` option corresponds to the target volume ratio.  
`iterations` option is the number of iteration used in the solver.

You can directly create a command using the following MEL code :  

    global string $ratioField;
    global string $iterField;
    
    global proc goCommand() {
    	global string $ratioField;
    	global string $iterField;
    				
    	inflate -ratio `floatField -query -value $ratioField` -iterations `intField -query -value $iterField`;
    }
    
    {
    	string $window = `window -widthHeight 160 60 -sizeable false -title "Inflate" -maximizeButton false -minimizeButton false`;
    	gridLayout -numberOfColumns 2 -cellWidthHeight 80 20;
    		text "volume ratio";
    		$ratioField = `floatField -precision 2 -minValue 0 -value 1 -step .01`;
    		text "iterations";
    		$iterField = `intField -minValue 1 -value 3 -maxValue 100`;
    		button -label "OK" -command goCommand;
    	showWindow $window;
    }
