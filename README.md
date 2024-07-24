# Email Read Receipt Generator

## Demo

https://github.com/user-attachments/assets/58cb1739-1ce7-414e-8e70-b2442e4cd89a

This project consists of two main components:
1. **Backend**: A server written in C, handling HTTPS and SMTP protocols to generate and send email read receipts.
2. **Extension**: A Chrome extension that allows users to track their emails by adding a tracking pixel before sending.

## Installation
### 1. Get the ```extension```.

#### If you have `Git` installed (See below for installation without using Git):
1. Open your `terminal` (`Command Prompt` on `Windows`)
2. Run the following:

```sh
mkdir my_extensions
cd my_extensions
git init
git remote add origin https://github.com/sulabhkatila/email-read-receipts-generator.git
git config core.sparseCheckout true
echo "extension" >> .git/info/sparse-checkout
git pull origin main
```

3. The `extension` directory is now on your computer. You can verify this by running:
```sh
ls
```

You should see the `extension` directory listed.

#### If you don't have `Git`:
1. Open your web browser and navigate to the GitHub repository: https://github.com/sulabhkatila/email-read-receipts-generator.

2. On the repository page, click the green "Code" button located near the top right corner.

3. In the dropdown menu, click "Download ZIP" to download the repository as a ZIP file.

4. Once the ZIP file is downloaded, extract its contents to a directory of your choice.

5. After extraction, navigate to the extracted folder and locate the extension directory.

### 2. Add the extension to chrome
1. Open Chrome and go to `chrome://extensions/` (`brave://extensions` for brave).
2. Enable `Developer mode` by clicking the toggle switch in the top right corner.
3. Click `Load unpacked` and select the extension directory.


## Usage
Email like you, normally, would. Just turn the receipts switch on before sending to get receipts.

- Receipts turned off:</br>
<img width="453" alt="Screenshot 2024-07-01 at 7 51 51 PM" src="https://github.com/sulabhkatila/Email-Read-Receipts/assets/113466992/b115c3d1-fd11-4c58-8596-f8e9b91e145d">

- Receipts turned on:</br>
<img width="449" alt="Screenshot 2024-07-01 at 7 52 11 PM" src="https://github.com/sulabhkatila/Email-Read-Receipts/assets/113466992/39a46f75-3ee6-468a-8a7c-ee64a66b4dad">

