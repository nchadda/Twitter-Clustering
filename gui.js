$(function() {
	
	document.getElementById('files').addEventListener('change', handleFileSelect, false);

	for (var i=0; i < 100; i++) {
		var column = $('<div class="column bin' + i + '"></div>');
		for (var j=0; j<20; j++) {

			var cell = $('<div id="' + i + 'x' + j + '" class="cell"></div>');
			cell.text('Empty');
			column.prepend(cell);
		}
		$('.gridcontainer').append(column);
	}

	function handleFileSelect(evt) {
	    var files = evt.target.files; // FileList object

	      var reader = new FileReader();
	      var statsFile;
	      // Closure to capture the file information.
	      reader.onload = function (event) {
	      	statsFile = event.target.result;
	      	rows = statsFile.split('\n');
	      	for (x in rows) {
	      		row = rows[x];
	      		cellCoords = row.slice(row.indexOf('binx') + 4, row.indexOf(','));
	      		$('#' + cellCoords).text(row.substring(row.indexOf(',') + 1));

	      	}
	      }

	      reader.readAsText(files[0]);

	      
  	}


});