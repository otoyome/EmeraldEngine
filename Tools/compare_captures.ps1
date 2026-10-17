#	@tool 自動キャプチャ（-autotest）の画像を基準と比較する
#	@tool 使い方: powershell -File Tools\compare_captures.ps1 -Baseline Captures\baseline -Target Captures\current [-Threshold 16] [-MaxRatio 0.005]
#	@tool 画素のどれかの色成分の差が Threshold を超えたものを「違う画素」と数え、その割合が MaxRatio を超えた画像を不一致とする
#	@tool 不一致の画像は Target\diff に差分画像（差を強調したもの）を出力する。不一致があれば終了コード 1
param(
	[Parameter(Mandatory = $true)][string]$Baseline,
	[Parameter(Mandatory = $true)][string]$Target,
	[int]$Threshold = 16,
	[double]$MaxRatio = 0.005
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class CaptureComparer
{
	public static double[] Compare(string basePath, string targetPath, int threshold, string diffPath)
	{
		using (var a = new Bitmap(basePath))
		using (var b = new Bitmap(targetPath))
		{
			if (a.Width != b.Width || a.Height != b.Height)
				return new double[] { -1, 0, 0 };

			var rect = new Rectangle(0, 0, a.Width, a.Height);
			var da = a.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
			var db = b.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
			int bytes = da.Stride * a.Height;
			var pa = new byte[bytes];
			var pb = new byte[bytes];
			Marshal.Copy(da.Scan0, pa, 0, bytes);
			Marshal.Copy(db.Scan0, pb, 0, bytes);
			a.UnlockBits(da);
			b.UnlockBits(db);

			var pd = new byte[bytes];
			long differ = 0;
			double sum = 0;
			int maxDiff = 0;
			for (int i = 0; i < bytes; i += 4)
			{
				int d = 0;
				for (int c = 0; c < 3; ++c)
				{
					int v = Math.Abs(pa[i + c] - pb[i + c]);
					sum += v;
					if (v > d) d = v;
				}
				if (d > maxDiff) maxDiff = d;
				if (d > threshold) ++differ;
				byte e = (byte)Math.Min(255, d * 8);
				pd[i] = e; pd[i + 1] = e; pd[i + 2] = e; pd[i + 3] = 255;
			}

			double pixels = (double)a.Width * a.Height;
			double ratio = differ / pixels;
			if (diffPath != null && differ > 0)
			{
				using (var diff = new Bitmap(a.Width, a.Height, PixelFormat.Format32bppArgb))
				{
					var dd = diff.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
					Marshal.Copy(pd, 0, dd.Scan0, bytes);
					diff.UnlockBits(dd);
					diff.Save(diffPath, ImageFormat.Png);
				}
			}
			return new double[] { ratio, sum / (pixels * 3), maxDiff };
		}
	}
}
"@

$failed = 0
$diffDirectory = Join-Path $Target 'diff'
New-Item -ItemType Directory -Force $diffDirectory | Out-Null

foreach ($baseImage in Get-ChildItem $Baseline -Filter *.png)
{
	$targetImage = Join-Path $Target $baseImage.Name
	if (-not (Test-Path $targetImage))
	{
		Write-Output ("MISSING  {0}" -f $baseImage.Name)
		++$failed
		continue
	}

	$diffPath = Join-Path $diffDirectory $baseImage.Name
	$result = [CaptureComparer]::Compare($baseImage.FullName, (Resolve-Path $targetImage).Path, $Threshold, $diffPath)
	if ($result[0] -lt 0)
	{
		Write-Output ("SIZE     {0}" -f $baseImage.Name)
		++$failed
		continue
	}

	$status = if ($result[0] -gt $MaxRatio) { 'NG'; ++$failed } else { 'OK' }
	Write-Output ("{0,-8} {1,-45} differ={2,8:P3} mean={3,7:F3} max={4,3}" -f $status, $baseImage.Name, $result[0], $result[1], $result[2])
}

exit ([int]($failed -gt 0))
