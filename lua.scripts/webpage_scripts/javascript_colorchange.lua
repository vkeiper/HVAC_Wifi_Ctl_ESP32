buf = buf.."<head> <script language=\"javascript\">";

buf = buf.."function colorchange(id) { var xmlhttp; xmlhttp=new XMLHttpRequest();";

buf = buf.."if(document.getElementById(id).value == \'OFF\' )";

buf = buf.."{ document.getElementById(id).style.background = \'#ff0000\'; ";

buf = buf.."document.getElementById(id).value = \'ON\';";

buf = buf.."xmlhttp.open(\"GET\",\"a.htm?pin=ON2\",true);xmlhttp.send();}";

buf = buf.."else { document.getElementById(id).style.background = \'#00ff00\'; ";

buf = buf.."document.getElementById(id).value = \'OFF\'; xmlhttp.open(\"GET\",\"a.htm?pin=OFF2\",true);xmlhttp.send();}}";

buf = buf.." </script></head> <body>";

buf = buf.."<input type=\"button\" id=\"btn\" onclick=\"colorchange(\'btn\');\" value =\"OFF\" style=\"background:#00ff00;\"></body></html>";

local _on,_off = "",""

if(_GET.pin == "ON1")then

gpio.write(led1, gpio.HIGH);

elseif(_GET.pin == "OFF1")then

gpio.write(led1, gpio.LOW);

elseif(_GET.pin == "ON2")then

gpio.write(led2, gpio.HIGH);

elseif(_GET.pin == "OFF2")then

gpio.write(led2, gpio.LOW);

end

client:send(buf);

client:close();

collectgarbage();

end)

end)