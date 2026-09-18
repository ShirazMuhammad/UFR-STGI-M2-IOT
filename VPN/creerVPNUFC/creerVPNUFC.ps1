# CreerVPNUFC.ps1

Add-VpnConnection -Name "VPN UFC" -ServerAddress "vpn20-2.univ-fcomte.fr" `
 -AuthenticationMethod EAP `
 -EncryptionLevel Required `
 -TunnelType IKEv2

Set-VpnConnection "VPN UFC" -SplitTunneling $True

Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 193.52.61.0/24 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 193.52.184.0/23 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 193.54.75.0/24 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 193.55.65.0/24 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 193.55.66.0/23 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 193.55.68.0/22 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 194.57.76.0/22 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 194.57.80.0/21 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 194.57.88.0/22 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 195.83.18.0/23 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 195.83.112.0/23 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 195.220.182.0/23 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 195.220.184.0/23 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 195.221.254.0/23 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 172.16.0.0/16 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 172.20.0.0/16 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 172.21.0.0/16 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 172.22.0.0/16 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 172.23.0.0/16 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 172.26.0.0/18 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 172.28.0.0/16 -PassThru
Add-VpnConnectionRoute -ConnectionName "VPN UFC" -DestinationPrefix 10.0.0.0/8 -PassThru

# Add-VpnConnectionTriggerDnsConfiguration -ConnectionName "VPN UFC" -DnsIpAddress "195.83.19.60" -DnsSuffix "femto-st.fr" -PassThru -Force

$pbkpath = "$env:APPDATA\Microsoft\Network\Connections\Pbk\rasphone.pbk"
(Get-Content -path $pbkpath -Raw) -Replace 'UseRasCredentials=1','UseRasCredentials=0' | Set-Content -path $pbkpath

