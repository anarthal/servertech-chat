import React from "react"
import Image from "next/image"
import boostLogo from "../public/boost.jpg"
import { headerbutton, container } from './header.module.css'

const Separator = () => {

}

export default function Header() {
    return (
        <div className={container}>
            <div style={{ display: 'flex' }}>
                <Image src={boostLogo} height={80} ></Image>
                <div style={{ flex: 1, display: 'flex', justifyContent: 'flex-end' }}>
                    <a className={headerbutton} href="https://github.com/anarthal/servertech-chat">Source code</a>
                    <a className={headerbutton} href="/">Docs</a>
                </div>
            </div>
        </div>
    )
}

