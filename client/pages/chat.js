import Head from 'next/head';
import styles from '../styles/Home.module.css';
import Header from '../components/header';
import { TextField, Button } from '@mui/material';
import { useState } from 'react';

function genRandomName() {
  const number = Math.floor(Math.random() * 999999);
  return `user-${number}`
}

export default function Home() {
  const userName = localStorage.getItem('servertech_username') || genRandomName()
  return (
    <>
      <Head>
        <title>BoostServerTech chat</title>
        <link rel="icon" href="/favicon.ico" />
      </Head>

      <div className="flex flex-col">
        <Header />
        <div className={styles.bodycontainer}>
          <p>Hello {userName}!</p>
        </div>
      </div>
    </>
  )
}
