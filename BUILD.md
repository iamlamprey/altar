## How To Build

The tldr is to just run the included `build/{os}` scripts. I've tried to streamline the process as much as possible, but detailed sections for each are below, including building the old HISE version.

### Windows

### Linux

### MacOS (Big Sur and Above)

Install Brew and Xcode command line tools:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew --version
xcode-select -p
```

Install CMake:

```bash
brew install cmake
cmake --version
```

Build universal macOS artifacts:

```bash
chmod +x ./build/macos.sh
./build/macos.sh -c Release
```

The script always builds first and copies the outputs to:

```text
binaries/release/Altar.app
binaries/release/Altar.vst3
binaries/release/Altar.component
```

After build, the same script continues the release pipeline only when the required environment variables are set:

1. If `MACOS_CODESIGN_IDENTITY` is set, it Developer ID signs the app, VST3, and AU.
2. If `MACOS_INSTALLER_IDENTITY` is set, it builds and signs `binaries/package/Altar-<version>-macOS.pkg`.
3. It verifies the pkg payload includes the expected app, VST3, and AU install paths.
4. If notarization credentials are set, it submits the pkg to Apple, staples the ticket, validates the staple, and runs a Gatekeeper install assessment.
5. If a required variable is missing for the next release step, the script stops and prints what to set.

### MacOS Developer ID Codesigning

Use this when preparing a Mac build for local release testing or distribution outside the Mac App Store. This signs the standalone app, VST3, AU, and final pkg installer, then notarizes and staples the pkg.

`Developer ID Application`: Signs `Altar.app`, `Altar.vst3`, and `Altar.component`
`Developer ID Installer`: Signs the final `.pkg`
`notarytool`: Submits the signed `.pkg` to Apple for notarization

#### 1. Sign in to Apple Developer

Sign in at:

```text
https://developer.apple.com/account
```

If running a VM / hackintosh, the developer page can brick / show all black. Just switch to Chrome or another browser.

#### 2. Create a Certificate Signing Request

On the Mac that will hold the signing key:

1. Open `Keychain Access`.
2. Choose `Keychain Access > Certificate Assistant > Request a Certificate From a Certificate Authority...`.
3. Enter the Apple Developer account email.
4. Use a recognizable common name, for example `Altar Developer ID Application 2026`.
5. Leave the CA email blank.
6. Select `Saved to disk`.
7. If prompted for key settings, use `2048 bits` and `RSA`.

This creates a `.certSigningRequest` file and stores the matching private key in the local login keychain. Keep that private key safe; the downloaded Apple certificate only works when paired with it.

#### 3. Create the Developer ID Application Certificate

In Apple Developer:

1. Open `Certificates, Identifiers & Profiles`.
2. Add a new certificate.
3. Select `Developer ID Application`.
4. Select `G2 Sub-CA (Xcode 11.4.1 or later)` if asked for the certificate intermediate.
5. Upload the `.certSigningRequest` file.
6. Download the generated `.cer` file.
7. Double-click the `.cer` to add it to Keychain.

In `Keychain Access`, check `login > My Certificates`. The certificate should be expandable and should show its private key underneath.

#### 4. Create the Developer ID Installer Certificate

Repeat the CSR flow above, but use a new recognizable common name, for example:

```text
Altar Developer ID Installer 2026
```

In Apple Developer:

1. Open `Certificates, Identifiers & Profiles`.
2. Add a new certificate.
3. Select `Developer ID Installer`.
4. Select `G2 Sub-CA (Xcode 11.4.1 or later)` if asked for the certificate intermediate.
5. Upload the new `.certSigningRequest` file.
6. Download the generated `.cer` file.
7. Double-click the `.cer` to add it to Keychain.

In `Keychain Access`, check `login > My Certificates`. The installer certificate should also be expandable and should show its private key underneath.

#### 5. Install the Developer ID G2 Intermediate if Needed

If Keychain says the certificate is not trusted, or codesigning fails with:

```text
Warning: unable to build chain to self-signed root for signer
errSecInternalComponent
```

install Apple's Developer ID G2 intermediate certificate:

1. Open `https://www.apple.com/certificateauthority/`.
2. Download `Developer ID - G2`.
3. Add it to Keychain, preferably the `System` keychain.
4. Set the Developer ID certificates back to `Use System Defaults`.

Do not rely on forcing the leaf certificate to `Always Trust`; fix the intermediate chain instead.

#### 6. Confirm the Signing Identities

Run:

```bash
security find-identity -v -p codesigning
security find-identity -v -p basic
```

Look for identities like:

```text
Developer ID Application: Your Name (TEAMID)
Developer ID Installer: Your Name (TEAMID)
```

Use the exact identity strings from these commands for the environment variables below.

#### 7. Store Notarization Credentials

Create an app-specific password for the Apple ID:

1. Open `https://appleid.apple.com`.
2. Go to `Sign-In and Security`.
3. Create an app-specific password.
4. Name it something recognizable, for example `Altar Notary`.

Store the notarization credentials in the macOS Keychain:

```bash
xcrun notarytool store-credentials "AltarNotary" \
  --apple-id "apple-id@example.com" \
  --team-id "TEAMID" \
  --password "xxxx-xxxx-xxxx-xxxx"
```

`TEAMID` is the value in parentheses at the end of the Developer ID certificate name (the same one in your codesigning certificates). Do not commit the Apple ID, Team ID, or app-specific password to the repo.

Confirm the stored profile works:

```bash
xcrun notarytool history --keychain-profile "AltarNotary"
```

If the command authenticates and returns history, the profile is ready.

#### 8. Persist Release Environment Variables

On modern macOS, the default shell is usually `zsh`. Add the release variables to `~/.zshrc` so they survive VM restarts:

```bash
nano ~/.zshrc
```

Add one export per line:

```bash
export MACOS_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export MACOS_INSTALLER_IDENTITY="Developer ID Installer: Your Name (TEAMID)"
export MACOS_NOTARY_PROFILE="AltarNotary"
```

Do not let the certificate identity wrap onto multiple lines. This is wrong:

```bash
export MACOS_CODESIGN_IDENTITY="Developer ID Application: Your Name
(TEAMID)"
```

Reload the shell profile:

```bash
source ~/.zshrc
```

Confirm each identity prints on one line:

```bash
printf '<%s>\n' "$MACOS_CODESIGN_IDENTITY"
printf '<%s>\n' "$MACOS_INSTALLER_IDENTITY"
printf '<%s>\n' "$MACOS_NOTARY_PROFILE"
```

If the Mac is using bash instead of zsh, put the same exports in `~/.bash_profile`.

#### 9. Build, Sign, Package, Notarize, and Staple

From the repo root:

```bash
export MACOS_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export MACOS_INSTALLER_IDENTITY="Developer ID Installer: Your Name (TEAMID)"
export MACOS_NOTARY_PROFILE="AltarNotary"
chmod +x ./build/macos.sh
./build/macos.sh -c Release
```

Instead of `MACOS_NOTARY_PROFILE`, you can use Apple ID notarization credentials:

```bash
export APPLE_ID="apple-id@example.com"
export APPLE_TEAM_ID="TEAMID"
export APPLE_APP_PASSWORD="xxxx-xxxx-xxxx-xxxx"
```

Prefer `MACOS_NOTARY_PROFILE` for normal use so credentials stay in Keychain instead of shell history.

The script builds, signs, packages, notarizes, and staples:

```text
binaries/release/Altar.app
binaries/release/Altar.vst3
binaries/release/Altar.component
binaries/package/Altar-<version>-macOS.pkg
```

The pkg is built as one root payload so the files land at:

```text
/Applications/Altar.app
/Library/Audio/Plug-Ins/VST3/Altar.vst3
/Library/Audio/Plug-Ins/Components/Altar.component
```

The pkg marks these bundles as non-relocatable so macOS Installer should not move them to a previous receipt location.

Expected success output includes:

```text
valid on disk
satisfies its Designated Requirement
macOS release complete
```

#### 10. Inspect the Signed Artifacts

```bash
codesign -dv --verbose=4 binaries/release/Altar.app
codesign -dv --verbose=4 binaries/release/Altar.vst3
codesign -dv --verbose=4 binaries/release/Altar.component
pkgutil --check-signature binaries/package/Altar-<version>-macOS.pkg
xcrun stapler validate binaries/package/Altar-<version>-macOS.pkg
spctl --assess --type install --verbose binaries/package/Altar-<version>-macOS.pkg
```

The artifact signing details should show the `Developer ID Application` authority. The pkg signature should show the `Developer ID Installer` authority. The build script also uses hardened runtime and timestamp options for the app/plugin bundles.

#### 11. Local Install Smoke Test

```bash
sudo installer -pkg binaries/package/Altar-<version>-macOS.pkg -target /
test -d /Applications/Altar.app
test -d /Library/Audio/Plug-Ins/VST3/Altar.vst3
test -d /Library/Audio/Plug-Ins/Components/Altar.component
open /Applications/Altar.app
```

Then scan/load the plugin in the target DAW.

------------------------------------------------------------

### HISE Version (0.3b & Below)

![](altar.jpg)

Built with HISE: https://hise.dev/  

https://github.com/christophhart/HISE

### Modifications to HISE Source Code

Altar depends on several small fixes to the HISE source. I recommend using my fork:

https://github.com/nytemairqt/HISE

### Dependencies

Altar uses multiple HISE ThirdParty nodes with their own dependencies. They are included in the /DspNetworks/ThirdParty/src/dependencies folder and are (should be) compatible with GPL-3:

JSON: https://github.com/nlohmann/json/  
math_approx: https://github.com/Chowdhury-DSP/math_approx  
RTNeural: https://github.com/jatinchowdhury18/RTNeural  
RTNeural-NAM: https://github.com/jatinchowdhury18/RTNeural-NAM  
Rubberband (0.3b & below): https://github.com/breakfastquay/rubberband
signalsmith-stretch (0.4b & above): https://github.com/Signalsmith-Audio/signalsmith-stretch
xsimd: https://github.com/xtensor-stack/xsimd  

Read [DEPENDENCIES](./DspNetworks/ThirdParty/src/dependencies/DEPENDENCIES.md) for more information.

### Windows

TBA

### MacOS

Built and tested with macOS Monterey 12.6.1.

#### Install XCode

Latest versions of XCode will likely cause problems when compiling HISE. 

I used 13.2, available here:

`https://xcodereleases.com`

You might need to use a later version depending on your version of macOS.

#### Clone & Setup HISE 

Use my fork, which includes a template patch to fix an SIMD related error.

`git clone https://github.com/nytemairqt/HISE.git`

Extract the SDK:

`HISE/tools/SDK/sdk.zip`

The ASIOSDK2.3 and VST3 SDK folders need to be in the SDK folder, NOT the auto-generated "sdk" folder created by unzipping:

```
HISE/tools/SDK/ASIOSDK2.3
HISE/tools/SD/VST3 SDK
```

#### Update Projucer File

Now open the Projucer `HISE/tools/projucer/projucer` and drag the HISE Jucer file:

`HISE/projects/standalone/HISE Standalone.jucer` inside the Projucer.

#### Save & Build HISE

Click "Save & Open In IDE"

Once XCode launches, click Product -> Scheme -> Edit Scheme.

Under Build Configuration, select "Release".

Product -> Clean Build Folder
Product -> Build

#### Clone Altar, Compile ThirdParty Nodes

`git clone https://github.com/nytemairqt/altar.git`

Start HISE, `File -> Load Project -> Altar Root Folder`

DON'T OPEN THE XML. Instead, compile the ThirdParty Nodes:

`Export -> Compile DSP networks as dll`

#### Open Altar Project for Exporting Plugin

Open HISE again, this time you can open the Altar.xml.

`Export -> Export as FX plugin`

This will create a Binaries subfolder, and attempt to compile the plugin:

`Altar/Binaries/`

### Linux

These are the modified instructions from David Healey's Bootcamp Cheat Sheet. I highly recommend signing up for the bootcamp:

https://audiodevschool.com/courses/hise-bootcamp/

Assuming a fresh install (I used Linux Mint 21):

#### Install prerequisites
```bash
sudo apt update
```

```bash
sudo apt-get -y install \
  build-essential \
  git \
  make \
  mold \
  binutils-gold \
  libfreetype6-dev \
  libx11-dev \
  libxinerama-dev \
  libxrandr-dev \
  libxcursor-dev \
  mesa-common-dev \
  libasound2-dev \
  freeglut3-dev \
  libxcomposite-dev \
  libcurl4-gnutls-dev \
  libgtk-3-dev \
  libjack-jackd2-dev \
  libwebkit2gtk-4.1-dev \
  libfftw3-dev
```

#### Alias Mold Linker & Setup FFTW

Edit bashrc:
`nano ~/.bashrc`

```
# Add these lines at the end:

# mold linker alias
alias gold="mold"

# fftw3
export FFTWROOT=/usr/lib/x86_64-linux-gnu
export CPATH=$FFTWROOT/include:$CPATH
export LIBRARY_PATH=$FFTWROOT/lib:$LIBRARY_PATH
export LD_LIBRARY_PATH=$FFTWROOT/lib:$LD_LIBRARY_PATH
```

Apply & verify changes:

`source ~/.bashrc`

`gold --version` (should say mold)

#### Install Source Code Pro Font

Download: https://fonts.google.com/specimen/Source+Code+Pro
Extract to: `~/.fonts/SourceCodePro`
Move fonts from `static/` subfolder to the top-level folder.

#### Clone & Setup HISE

Use my fork, which includes several minor fixes to resolve annoying gcc errors.

`git clone https://github.com/nytemairqt/HISE.git`

Extract the SDK:

`HISE/tools/SDK/sdk.zip`

The ASIOSDK2.3 and VST3 SDK folders need to be in the SDK folder, NOT the auto-generated "sdk" folder created by unzipping:

```
HISE/tools/SDK/ASIOSDK2.3
HISE/tools/SD/VST3 SDK
```

#### Update Projucer File

Now open the Projucer `HISE/tools/projucer/projucer` and drag the HISE Jucer file:

`HISE/projects/standalone/HISE Standalone.jucer` inside the Projucer.

#### Enable FFTW3: 

Add to Extra Preprocessor Definitions:

```
AUDIOFFT_FFTW3=1
HI_ENABLE_CUSTOM_NODES=1
```

Add to Extra Linker Flags:

`-lfftw3f`

Enable in modules -> juce_dsp:

`JUCE_DSP_USE_STATIC_FFTW`

#### Save & Build HISE

Save Projucer project (File -> Save)

```
cd projects/standalone/Builds/LinuxMakefile
make clean
make CONFIG=Release -j 4
```

#### Fix Compiler Settings

Ctrl + H to view Hidden Files.

`~/.config/HISE/compilerSettings.xml`

Add the HISE Root folder to the HisePath variable.

Then copy the entire file to:

`~/.hise/compilerSettings.xml`

#### Open Altar Project for Compiling ThirdParty Nodes

Start HISE, `File -> Load Project -> Altar Root Folder`

DON'T OPEN THE XML. Instead, compile the ThirdParty Nodes:

`Export -> Compile DSP networks as dll`

This will generate: 

`Altar/DspNetworks/Binaries/`

Run `./batchCompileLinux.sh` to compile the ThirdParty Nodes.

#### Open Altar Project for Exporting Plugin

Open HISE again, this time you can open the Altar.xml.

`Export -> Export as FX plugin`

This will create a Binaries subfolder:

`Altar/Binaries/`

Open the directory, and run the `./batchCompileLinux.sh` script here to export the plugin. 

The final export is the ENTIRE Altar.vst3 folder.

### Non-AVX / Embedded CPUs

Altar is capable of running on non-AVX & embedded CPUs, just start compilation as usual:

`Export -> Export as FX plugin`

Cancel the operation with CTRL + C, then open the Jucer file:

`altar/Binaries/AutogeneratedProject.jucer`

Select the relevant IDE settings, and remove this from "Extra Compiler Flags":

`/arch:AVX`

Then compile in IDE. There might be some post-build errors but we can safely ignore them. 

You can also try enabling:

`Legacy CPU Support`

in the HISE Project Settings, but it never made a difference in my tests.
