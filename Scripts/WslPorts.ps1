# Based off https://github.com/microsoft/WSL/issues/4150#issuecomment-1018524753

# Variables
$localPorts=@(443,1883,5672,8080,8081,8883);
$remoteAddress = Invoke-Expression "wsl -d 'Ubuntu-24.04' hostname -I";
$firewallName = 'WSL Port Forwarding'
$localAddress='0.0.0.0';
$localPortsAsString = $localPorts -join ",";


# Remove previous firewall rules
Invoke-Expression "Remove-NetFireWallRule -DisplayName '$firewallName' ";

# Adding back firewall exception rules for inbound and outbound Rules
Invoke-Expression  "New-NetFireWallRule -DisplayName '$firewallName' -Direction Outbound -LocalPort $localPortsAsString -Action Allow -Protocol TCP";
Invoke-Expression  "New-NetFireWallRule -DisplayName '$firewallName' -Direction Inbound -LocalPort $localPortsAsString -Action Allow -Protocol TCP";

for ($i = 0; $i -lt $localPorts.length; $i++) {
  $localPort = $localPorts[$i];
  Invoke-Expression  "netsh interface portproxy delete v4tov4 listenport=$localPort listenaddress=$localAddress";
  Invoke-Expression  "netsh interface portproxy add v4tov4 listenport=$localPort listenaddress=$localAddress connectport=$localPort connectaddress=$remoteAddress";
}