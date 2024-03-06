Add-Type -Path "C:\Program Files (x86)\KAT Gateway\IBizLibrary.dll"
$data = New-Object IBizLibrary.KATSDKInterfaceHelper+TreadMillData 

$httpListener = New-Object System.Net.HttpListener
$httpListener.Prefixes.Add("http://localhost:9191/")
$httpListener.Start()

write-host "Press any key to stop the HTTP listener after next request"
while (!([console]::KeyAvailable)) {
  $context = $httpListener.GetContext()
  $context.Response.StatusCode = 200
  $context.Response.ContentType = "application/json"
  [IBizLibrary.KATSDKInterfaceHelper]::GetWalkStatus([ref]$data, $null)
  $WebContent = ConvertTo-Json $data
  $EncodingWebContent = [Text.Encoding]::UTF8.GetBytes($WebContent)
  $context.Response.OutputStream.Write($EncodingWebContent , 0, $EncodingWebContent.Length)
  $context.Response.Close()
  Write-Host -NoNewLine "."
}
$httpListener.Close()
