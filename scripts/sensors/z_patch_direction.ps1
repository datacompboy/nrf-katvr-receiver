$in_line = 0
$Input | ForEach-Object {
  $in_line++
  $skip = 0
  if ($in_line -eq   341) {
    if ($_ -ne ':2020F00028110020542D00206E48B0F90400258321846284A0846C484FF47A7127E6C04627') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   342) {
    if ($_ -ne ':20211000142009400120A071BBE0C046241F00200FF036F905200090189AA06B29461423B6') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   343) {
    if ($_ -ne ':202130000FF0F7F80FF032F9B078082840F0A980ACE7012903D0A079012840F0A28004F0AE') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   343) {
    Write-Output ':2020F00028110020542D00206E48B0F90400258321846284A0846C484FF47A7127E6C04627'
  }
  if ($in_line -eq   343) {
    Write-Output ':20211000142009400020A071BBE0C046241F00200FF036F905200090189AA06B29461423B7'
  }
  if ($in_line -eq   343) {
    Write-Output ':202130000FF0F7F80FF032F9B078082840F0A980ACE7012903D0A079012840F0A28004F0AE'
  }
  if ($in_line -eq   919) {
    if ($_ -ne ':206930008D70616ECF70616E0D71616E4F7100950E49E28A04F1600310201E46FFF70AFB21') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   920) {
    if ($_ -ne ':20695000C0B218B130461B210BF00EFA608B401C80B2B0F5FA7F18BF051C6583F8BDC04605') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   921) {
    if ($_ -ne ':20697000C80C0020341E0020782E002045000110F8000E40F0B5514C20780D46E90BADF180') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   921) {
    Write-Output ':206930008D70616ECF70616E0D71616E4F7100950E49E28A04F1600310201E46FFF70AFB21'
  }
  if ($in_line -eq   921) {
    Write-Output ':20695000C0B218B130461B210BF00EFA608B401C80B2B0F5FA7F18BF051C65830CF058BEAE'
  }
  if ($in_line -eq   921) {
    Write-Output ':20697000C80C0020341E0020782E002045000110F8000E40F0B5514C20780D46E90BADF180'
  }
  if ($in_line -eq   970) {
    if ($_ -ne ':206F90000BB90B46338001F0FF0003F0FF03C01A022806DB7088401C80B23228D8BF7080E8') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   971) {
    if ($_ -ne ':206FB00001DD318074807C803878002891D178680AF0DEFE8DE7C04684310100782E00205C') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   972) {
    if ($_ -ne ':206FD000341E0020BC210020BF210020BE210020281100208C2901008C2E002034130020E3') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   972) {
    Write-Output ':206F90000BB90B46338001F0FF0003F0FF03C01A022806DB7088401C80B23228D8BF7080E8'
  }
  if ($in_line -eq   972) {
    Write-Output ':206FB00001DD318074807C803878002891E778680AF0DEFE8DE7C04684310100782E002046'
  }
  if ($in_line -eq   972) {
    Write-Output ':206FD000341E0020BC210020BF210020BE210020281100208C2901008C2E002034130020E3'
  }
  if ($in_line -eq  2529) {
    if ($_ -ne ':203220000057000100000FAE157FAC1004AC5F0CB7010203AE68C115AF4B7BBE00F8020137') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2530) {
    if ($_ -ne ':20324000060302B12F1906FF094B415456520512BF08000900020AB4CFC107B03845BF0D9D') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2531) {
    if ($_ -ne ':20326000F3FFDD0783A505771084517F11F9E885C9FFE015080100C5F10F863B175FAF0286') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2531) {
    Write-Output ':203220000057000100000FAE157FAC1004AC5F0CB7010203AE68C115AF4B7BBE00F8020137'
  }
  if ($in_line -eq  2531) {
    Write-Output ':20324000060302B12F1906FF094B41542D440512BF08000900020AB4CFC107B03845BF0DD4'
  }
  if ($in_line -eq  2531) {
    Write-Output ':20326000F3FFDD0783A505771084517F11F9E885C9FFE015080100C5F10F863B175FAF0286'
  }
  if ($in_line -eq  2560) {
    if ($_ -ne ':14360000F00100209E350100F8010020A535010004020020B7') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2561) {
    if ($_ -ne ':20E00000FF0000000000000000000000000000000000000000000000000000000000000001') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2561) {
    Write-Output ':14360000F00100209E350100F8010020A535010004020020B7'
  }
  if ($in_line -eq  2561) {
    Write-Output ':14362000034D2878002802D16868FEF7A9FBF8BD2811002034'
  }
  if ($in_line -eq  2561) {
    Write-Output ':20E00000FF0000000000000000000000000000000000000000000000000000000000000001'
  }
  if ($skip -eq 0) {
    Write-Output $_
  }
}