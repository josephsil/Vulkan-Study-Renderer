Set-StrictMode -Version 3.0
$Lookup = Import-Csv ".\shader_struct_types.txt"
$LookuphashTable = @{}
$Lookup | Foreach-Object {$LookuphashTable[$_.hlsl] = $_.cpp}

Foreach($k in $($LookuphashTable.Keys))
{
    Write-Host $k : $LookuphashTable[$k]
}

$contents =  Get-content ".\Shaders\structs.hlsl" -Raw 

Write-Host $contents 
Foreach($k in $($LookuphashTable.Keys))
{
   $contents = $contents -replace "$k ", "$($LookuphashTable[$k]) "
}

Write-Host $contents

$structNameRegex = "(?<=struct)\s+(\S*)\s*(?={)"
Write-Host ".."
$structnames = (Select-String -InputObject $contents -Pattern $structNameRegex -AllMatches).Matches

Foreach($s in $structnames)
{
    $sname = $s.ToString().trim()
    $contents = $contents -replace $("(?-i)$sname"), "GPU_$sname"
}
Write-Host $contents

$output = "//GENERATED FILE`n//EDIT structs.hlsl TO UPDATE`n#pragma once"+ "`n"+'#include <Renderer/VulkanIncludes/forward-declarations-renderer.h>'+"`n"+ '#include <General/GLM_IMPL.h>'+ "`n"
$output += $contents

Write-Host $output 
$output | Out-File Code/Renderer/VulkanIncludes/structs.hlsl.h