
<script> 
    var an_CndTempRaw;
	var an_AmbTempRaw;
	
	function GetGaugeData()
	{
		nocache = "&nocache=" + Math.random() * 1000000;
		var request = new XMLHttpRequest();
		request.onreadystatechange = function()
		{
			if (this.readyState == 4) {
				if (this.status == 200) {
					if (this.responseXML != null) {
						an_CndTempRaw = this.responseXML.getElementsByTagName('an_CndTemp')[0].childNodes[0].nodeValue;
						an_AmbTempRaw = this.responseXML.getElementsByTagName('an_AmbTemp')[0].childNodes[0].nodeValue;
					}
				}
			}
		}
		request.open("GET", "ajax_temp" + nocache, true);
		request.send(null);
		//setTimeout('GetGaugeData()', 1000);
	}
</script>

